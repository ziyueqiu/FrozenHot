/*
 * Copyright (c) 2014 Tim Starling
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef incl_FIFO_CACHE_FH_H
#define incl_FIFO_CACHE_FH_H

#include <tbb/concurrent_hash_map.h>
#include "../hash/hash_header.h"
#include "FHCache.h"

#include <atomic>
#include <mutex>
#include <new>
#include <thread>
#include <iostream>
#include <string>
#include <vector>
// stat
#define FH_STAT
#define HANDLE_WRITE
#define MARKER_KEY 2 // 2 is reserved for marker key
#define SPECIAL_KEY 0 // 0 is reserved for special key
#define TOMB_KEY 1 // 1 is reserved for tomb key
#define FC_RELAXATION 20 // TODO @ Ziyue: remove it later

namespace Cache {
/**
 * FIFO_FHCache is a thread-safe hashtable with a limited size. When
 * it is full, insert() evicts the least recently used item from the cache.
 *
 * The find() operation fills a ConstAccessor object, which is a smart pointer
 * similar to TBB's const_accessor. After eviction, destruction of the value is
 * deferred until all ConstAccessor objects are destroyed.
 *
 * The implementation is generally conservative, relying on the documented
 * behaviour of tbb::concurrent_hash_map. LRU list transactions are protected
 * with a single mutex. Having our own doubly-linked list implementation helps
 * to ensure that list transactions are sufficiently brief, consisting of only
 * a few loads and stores. User code is not executed while the lock is held.
 *
 * The acquisition of the list mutex during find() is non-blocking (try_lock),
 * so under heavy lookup load, the container will not stall, instead some LRU
 * update operations will be omitted.
 *
 * Insert performance was observed to degrade rapidly when there is a heavy
 * concurrent insert/evict load, mostly due to locks in the underlying
 * TBB::CHM. So if that is a possibility for your workload,
 * ThreadSafeScalableCache is recommended instead.
 */
template <class TKey, class TValue, class THash = tbb::tbb_hash_compare<TKey>>
class FIFO_FHCache : public FHCacheAPI<TKey, TValue, THash> {
  /**
   * The LRU list node.
   *
   * We make a copy of the key in the list node, allowing us to find the
   * TBB::CHM element from the list node. TBB::CHM invalidates iterators
   * on most operations, even find(), ruling out more efficient
   * implementations.
   */
  struct ListNode {
    ListNode() : m_key(SPECIAL_KEY), m_time(0), m_prev(OutOfListMarker), m_next(nullptr) {}

    ListNode(const TKey& key)
        : m_key(key), m_time(SPDK_TIME), m_prev(OutOfListMarker), m_next(nullptr) {}

    TKey m_key;
    uint64_t m_time;
    ListNode* m_prev;
    ListNode* m_next;

    bool isInList() const { return m_prev != OutOfListMarker; }
  };

  static ListNode* const OutOfListMarker;

  /**
   * The value that we store in the hashtable. The list node is allocated from
   * an internal object_pool. The ListNode* is owned by the list.
   */
  struct HashMapValue {
    HashMapValue() : m_listNode(nullptr) {}

    HashMapValue(const TValue& value, ListNode* node)
        : m_value(value), m_listNode(node) {}

    TValue m_value;
    ListNode* m_listNode;
  };

  typedef tbb::concurrent_hash_map<TKey, HashMapValue, THash> HashMap;
  typedef typename HashMap::const_accessor HashMapConstAccessor;
  typedef typename HashMap::accessor HashMapAccessor;
  typedef typename HashMap::value_type HashMapValuePair;
  typedef std::pair<const TKey, TValue> SnapshotValue;

 public:
  /**
   * The proxy object for TBB::CHM::const_accessor. Provides direct access to
   * the user's value by dereferencing, thus hiding our implementation
   * details.
   */
  struct ConstAccessor {
    ConstAccessor() {}

    const TValue& operator*() const { return *get(); }

    const TValue* operator->() const { return get(); }

    // const TValue* get() const {
    //   return &m_hashAccessor->second.m_value;
    // }
    const TValue get() const { return m_hashAccessor->second.m_value; }

    bool empty() const { return m_hashAccessor.empty(); }

   private:
    friend class FIFO_FHCache;
    HashMapConstAccessor m_hashAccessor;
    //HashMapAccessor m_hashAccessor;
  };

  /**
   * Create a container with a given maximum size
   */
  explicit FIFO_FHCache(size_t maxSize);

  FIFO_FHCache(const FIFO_FHCache& other) = delete;
  FIFO_FHCache& operator=(const FIFO_FHCache&) = delete;

  virtual ~FIFO_FHCache() { clear(); }

  void thread_init(int tid) override {
    m_fasthash->thread_init(tid);
  }

  /**
   * Find a value by key, and return it by filling the ConstAccessor, which
   * can be default-constructed. Returns true if the element was found, false
   * otherwise. Updates the eviction list, making the element the
   * most-recently used.
   */
  //bool find(ConstAccessor& ac, const TKey& key);
  virtual bool find(TValue& ac, const TKey& key) override;
  virtual bool find_marker(TValue& ac, const TKey& key) override;

  /**
   * Insert a value into the container. Both the key and value will be copied.
   * The new element will put into the eviction list as the most-recently
   * used.
   *
   * If there was already an element in the container with the same key, it
   * will not be updated, and false will be returned. Otherwise, true will be
   * returned.
   */
  virtual bool insert(const TKey& key, const TValue& value) override;

  /**
   * Clear the container. NOT THREAD SAFE -- do not use while other threads
   * are accessing the container.
   */
  virtual void clear() override;

  /**
   * Get a snapshot of the keys in the container by copying them into the
   * supplied vector. This will block inserts and prevent LRU updates while it
   * completes. The keys will be inserted in order from most-recently used to
   * least-recently used.
   */
  virtual void snapshotKeys(std::vector<TKey>& keys) override;

  /**
   * Get the approximate size of the container. May be slightly too low when
   * insertion is in progress.
   */
  virtual size_t size() const override { return m_size.load(); }

  virtual bool is_full() override { return m_size.load() >= m_maxSize; }

  virtual void delete_key(const TKey& key) override;

  //void evict_key();

  //virtual bool construct_from(const TKey& key) override;
  virtual bool construct_ratio(double FC_ratio) override;
  virtual bool construct_tier() override;
  virtual void deconstruct() override;
  virtual bool get_curve(bool& should_stop) override;
  void debug(int status);
  bool tier_ready = false;

 private:
  /**
   * Unlink a node from the list. The caller must lock the list mutex while
   * this is called.
   */
  void delink(ListNode* node);

  /**
   * Add a new node to the list in the most-recently used position. The caller
   * must lock the list mutex while this is called.
   */
  void pushFront(ListNode* node);

  void pushAfter(ListNode* nodeBefore, ListNode* nodeAfter);


  /**
   * Evict the least-recently used item from the container. This function does
   * its own locking.
   */
  bool evict();

  /**
   * The maximum number of elements in the container.
   */
  size_t m_maxSize;

  /**
   * This atomic variable is used to signal to all threads whether or not
   * eviction should be done on insert. It is approximately equal to the
   * number of elements in the container.
   */
  std::atomic<size_t> m_size;

  /**
   * The underlying TBB hash map.
   */
  HashMap m_map;

  std::unique_ptr<FastHash::FashHashAPI<TValue>>  m_fasthash;

  /**
   * The linked list. The "head" is the most-recently used node, and the
   * "tail" is the least-recently used node. The list mutex must be held
   * during both read and write.
   */
  ListNode m_head;
  ListNode m_tail;
  typedef std::mutex ListMutex;
  ListMutex m_listMutex;

  bool fast_hash_ready = false;
  bool fast_hash_construct = false;
  std::atomic<bool> tier_no_insert{false};
  std::atomic<bool> curve_flag{false};
  std::atomic<size_t> movement_counter{0};
  std::atomic<size_t> eviction_counter{0};

  size_t unreachable_counter = 0;

  ListNode m_fast_head;
  ListNode m_fast_tail;
  ListNode *m_marker = nullptr;
};

template <class TKey, class TValue, class THash>
typename FIFO_FHCache<TKey, TValue, THash>::ListNode* const
    FIFO_FHCache<TKey, TValue, THash>::OutOfListMarker = (ListNode*)-1;

template <class TKey, class TValue, class THash>
FIFO_FHCache<TKey, TValue, THash>::FIFO_FHCache(size_t maxSize)
    : m_maxSize(maxSize),
      m_size(0),
      m_map(std::thread::hardware_concurrency() *
            4)  // it will automatically grow
{
  m_head.m_prev = nullptr;
  m_head.m_next = &m_tail;
  m_tail.m_prev = &m_head;

  m_fast_head.m_prev = nullptr;
  m_fast_head.m_next = &m_fast_tail;
  m_fast_tail.m_prev = &m_fast_head;

  int align_len = 1 + int(log2(m_maxSize));
  m_fasthash.reset(new FastHash::CLHT<TValue>(0, align_len));
  //m_fasthash.reset(new FastHash::TbbCHT<TValue>(m_maxSize));
}

template <class TKey, class TValue, class THash>
bool FIFO_FHCache<TKey, TValue, THash>::construct_ratio(double FC_ratio) {
  assert(FC_ratio <= 1 && FC_ratio >=0);
  assert(m_fast_head.m_next == &m_fast_tail);
  assert(m_fast_tail.m_prev == &m_fast_head);
  if(eviction_counter.load() > 0){
    //printf("\neviction counter error(?): %lu\n", eviction_counter.load());
    eviction_counter = 0;
  }

  m_fast_head.m_next = m_head.m_next;
  fast_hash_construct = true;

  size_t FC_size = FC_ratio * m_maxSize;
  size_t DC_size = m_maxSize - FC_size;
  printf("FC_size: %lu, DC_size: %lu\n", FC_size, DC_size);
  size_t fail_count = 0, count = 0;
  bool first_pass_flag = true;
  ListNode* temp_node = m_fast_head.m_next;
  ListNode* delete_temp;
  HashMapConstAccessor temp_hashAccessor;

  while(temp_node != &m_fast_tail){
    count++;
    auto eviction_count = eviction_counter.load();
    if(! m_map.find(temp_hashAccessor, temp_node->m_key)){
      delete_temp = temp_node;
      //printf("fail key No.%lu: %lu, count: %lu\n", fail_count, delete_temp->m_key, count);
      //printf("insert count: %lu v.s. eviction count: %lu\n", count, eviction_count);
      temp_node = temp_node->m_next;
      if(delete_temp->isInList())
        delink(delete_temp);
      delete delete_temp;
      fail_count++;
      continue;
    }
    m_fasthash->insert(temp_node->m_key, temp_hashAccessor->second.m_value);
    temp_node = temp_node->m_next;

    if(count > FC_size - FC_RELAXATION && first_pass_flag == true){
      std::unique_lock<ListMutex> lock(m_listMutex);
      // m_fast_head.m_next is right
      auto nodeBefore = m_fast_head.m_next->m_prev;
      auto nodeAfter = temp_node;
      m_fast_tail.m_prev = temp_node->m_prev;
      temp_node->m_prev->m_next = &m_fast_tail;
      m_fast_head.m_next->m_prev = &m_fast_head;
      nodeBefore->m_next = nodeAfter;
      nodeAfter->m_prev = nodeBefore;
      lock.unlock();
      break;
    } else if(eviction_count > DC_size - FC_RELAXATION && first_pass_flag == true) {
      std::unique_lock<ListMutex> lock(m_listMutex);
      // m_fast_head.m_next is right
      auto node = m_fast_head.m_next;
      m_fast_tail.m_prev = m_tail.m_prev;
      m_tail.m_prev->m_next = &m_fast_tail;
      m_tail.m_prev = node->m_prev;
      m_tail.m_prev->m_next = &m_tail;
      node->m_prev = &m_fast_head;
      lock.unlock();
      first_pass_flag = false;
    }
  }
  if(fail_count > 0)
    printf("fast hash insert num: %lu, fail count: %lu, m_size: %ld (FC_ratio: %.2lf)\n", 
        count, fail_count, m_size.load(), count*1.0/m_size.load());
  else
    printf("fast hash insert num: %lu, m_size: %ld (FC_ratio: %.2lf)\n", 
        count, m_size.load(), count*1.0/m_size.load());
  fast_hash_ready = true;
  fast_hash_construct = false;
  eviction_counter = 0;
  return true;
}

// template <class TKey, class TValue, class THash>
// bool FIFO_FHCache<TKey, TValue, THash>::construct_from(const TKey& key) {
//   HashMapConstAccessor hashAccessor;
//   if (!m_map.find(hashAccessor, key)) {
//     return false;
//   }
//   std::unique_lock<ListMutex> lock(m_listMutex);
//   fast_hash_construct = true;

//   ListNode* node = hashAccessor->second.m_listNode;
//   if (!node->isInList()) {
//     printf("not possible\n");
//     return false;
//   }

//   assert(m_fast_head.m_next == &m_fast_tail);
//   assert(m_fast_tail.m_prev == &m_fast_head);

//   m_fast_head.m_next = m_head.m_next;
//   m_head.m_next->m_prev = &m_fast_head;
//   m_fast_tail.m_prev = node->m_prev;
//   node->m_prev->m_next = &m_fast_tail;

//   m_head.m_next = node->m_next;
//   node->m_next->m_prev = &m_head;
//   lock.unlock();

//   m_map.erase(hashAccessor);
//   delete node;
//   m_size--;
  
//   // insert into fast hash
//   int count = 0;
//   int fail_count = 0;
//   ListNode* temp_node = m_fast_head.m_next;
//   ListNode* delete_temp;
//   HashMapConstAccessor temp_hashAccessor;
//   while(temp_node != &m_fast_tail){
// #ifdef HANDLE_WRITE
//     if(temp_node->m_key == TOMB_KEY) {
//       delete_temp = temp_node;
//       temp_node = temp_node->m_next;
//       delink(delete_temp);
//       delete delete_temp;
//       continue;
//     }
// #endif
//     if(! m_map.find(temp_hashAccessor, temp_node->m_key)){
//       delete_temp = temp_node;
//       temp_node = temp_node->m_next;
//       if(delete_temp->isInList())
//         delink(delete_temp);
//       delete delete_temp;
//       fail_count++;
//       continue;
//     }
// #ifdef HANDLE_WRITE
//     if(temp_node->m_key == TOMB_KEY) {
//       delete_temp = temp_node;
//       temp_node = temp_node->m_next;
//       delink(delete_temp);
//       delete delete_temp;
//       continue;
//     }
// #endif
//     m_fasthash->insert(temp_node->m_key, temp_hashAccessor->second.m_value);
//     count++;
//     temp_node = temp_node->m_next;
//   }
//   if(fail_count > 0)
//     printf("fast hash insert num: %d, fail count: %d, m_size: %ld (FC_ratio: %.2lf)\n", 
//         count, fail_count, m_size.load(), count*1.0/m_size.load());
//   else
//     printf("fast hash insert num: %d, m_size: %ld (FC_ratio: %.2lf)\n", 
//         count, m_size.load(), count*1.0/m_size.load());
//   fast_hash_ready = true;
//   fast_hash_construct = false;
//   return true;
// }

template <class TKey, class TValue, class THash>
bool FIFO_FHCache<TKey, TValue, THash>::construct_tier() {
  std::unique_lock<ListMutex> lock(m_listMutex);
  fast_hash_construct = true;
  tier_no_insert = true;
  assert(m_fast_head.m_next == &m_fast_tail);
  assert(m_fast_tail.m_prev == &m_fast_head);
  m_fast_head.m_next = m_head.m_next;
  m_head.m_next->m_prev = &m_fast_head;
  m_fast_tail.m_prev = m_tail.m_prev;
  m_tail.m_prev->m_next = &m_fast_tail;
  m_head.m_next = &m_tail;
  m_tail.m_prev = &m_head;

  lock.unlock();

  int count = 0;
  ListNode* temp_node = m_fast_head.m_next;
  ListNode* delete_temp;
  HashMapConstAccessor temp_hashAccessor;
  while(temp_node != &m_fast_tail){
#ifdef HANDLE_WRITE
    if(temp_node->m_key == TOMB_KEY) {
      delete_temp = temp_node;
      temp_node = temp_node->m_next;
      delink(delete_temp);
      delete delete_temp;
      continue;
    }
#endif
    if(! m_map.find(temp_hashAccessor, temp_node->m_key)){
      delete_temp = temp_node;
      temp_node = temp_node->m_next;
      if(delete_temp->isInList()){
        delink(delete_temp);
      }
      delete delete_temp;
      continue;
    }
#ifdef HANDLE_WRITE
    if(temp_node->m_key == TOMB_KEY) {
      delete_temp = temp_node;
      temp_node = temp_node->m_next;
      delink(delete_temp);
      delete delete_temp;
      continue;
    }
#endif
    m_fasthash->insert(temp_node->m_key, temp_hashAccessor->second.m_value);
    count++;
    temp_node = temp_node->m_next;
  }
  printf("fast hash insert num: %d, m_size: %ld (FC_ratio: %.2lf)\n", 
      count, m_size.load(), count*1.0/m_size.load());
  tier_ready = true;
  fast_hash_construct = false;
  return true;
}

template <class TKey, class TValue, class THash>
void FIFO_FHCache<TKey, TValue, THash>::debug(int status) {
  if(status == 0){
    ListNode* temp = m_head.m_next;
    int count = 0;
    while(temp != &m_tail){
      count++;
      assert(temp != nullptr);
      temp = temp->m_next;
    }
    printf("Main LRU list Size: %d, m_size: %ld\n", count, m_size.load());
  }
  else if(status == 1){
    ListNode* temp = m_fast_head.m_next;
    int count = 0;
    while(temp != &m_fast_tail){
      count++;
      assert(temp != nullptr);
      temp = temp->m_next;
    }
    printf("Fast LRU list Size: %d, m_size: %ld\n", count, m_size.load());
  }
  else if(status == 2){
    std::unique_lock<ListMutex> lock(m_listMutex);
    ListNode* temp = m_head.m_next;
    int count = 0;
    while(temp != &m_tail){
      count++;
      assert(temp != nullptr);
      temp = temp->m_next;
    }
    printf("Main LRU list Size: %d, m_size: %ld\n", count, m_size.load());
  }
}

template <class TKey, class TValue, class THash>
void FIFO_FHCache<TKey, TValue, THash>::deconstruct() {
  assert(!((m_fast_head.m_next == &m_fast_tail) ^ (m_fast_tail.m_prev == &m_fast_head)));
  if(m_fast_head.m_next == &m_fast_tail){
    printf("no need to deconstruct!\n");
    fflush(stdout);
    return;
  }

  std::unique_lock<ListMutex> lock(m_listMutex);
  ListNode* node = m_head.m_next;
  m_fast_head.m_next->m_prev = &m_head;
  m_fast_tail.m_prev->m_next = node;
  m_head.m_next = m_fast_head.m_next;
  node->m_prev = m_fast_tail.m_prev;

  m_fast_head.m_next = &m_fast_tail;
  m_fast_tail.m_prev = &m_fast_head;

  if(tier_ready){
    tier_no_insert = false;
    tier_ready = false;
  }
  if(fast_hash_ready)
    fast_hash_ready = false;
  lock.unlock();

  m_fasthash->clear();
}

template <class TKey, class TValue, class THash>
bool FIFO_FHCache<TKey, TValue, THash>::find(TValue& ac,
                                                   const TKey& key) {
  bool stat_yes = FIFO_FHCache::sample_generator();
  HashMapConstAccessor hashAccessor;
  //HashMapAccessor& hashAccessor = ac.m_hashAccessor;
  assert(!(tier_ready || fast_hash_ready) || !curve_flag.load());

  if(tier_ready || fast_hash_ready) {
#ifdef HANDLE_WRITE
    if(m_fasthash->find(key, ac) && (ac != nullptr))
#else
    if(m_fasthash->find(key, ac))
#endif
    {

#ifdef FH_STAT
      if(stat_yes) {
        FIFO_FHCache::fast_find_hit++;
      }
#endif
      return true;
    }
    else {
      if(tier_ready){
#ifdef FH_STAT
        if(stat_yes) {
          FIFO_FHCache::tbb_find_miss++;
        }
#endif
        return false;
      }
    }
  }

  if (!m_map.find(hashAccessor, key)) {
#ifdef FH_STAT
    if(stat_yes) {
      FIFO_FHCache::tbb_find_miss++;
    }
#endif
    return false;
  }
  ac = hashAccessor->second.m_value;
  if(!fast_hash_construct) {
    // Acquire the lock, but don't block if it is already held
    ListNode* node = hashAccessor->second.m_listNode;
    uint64_t last_update = 0;
    if(curve_flag.load()){
      // TODO @ Ziyue: there is no lock, so corner case is that
      // curve_flag turns into false, m_marker might cause core dump
      last_update = node->m_time;
      if(m_marker != nullptr && last_update <= m_marker->m_time){
        if(stat_yes){
          FIFO_FHCache::end_to_end_find_succ++;
        }
        movement_counter++;
        node->m_time = SPDK_TIME;
        std::unique_lock<ListMutex> lock(m_listMutex);
        if(node->isInList()) {
          delink(node);
          pushFront(node);
        }
      }
      else if(stat_yes){
        FIFO_FHCache::fast_find_hit++;
      }
      return true;
    }
    
    //if(((double)(ssdlogging::random::Random::GetTLSInstance()->Next()) / (double)(RAND_MAX)) < LOCK_P){
    // std::unique_lock<ListMutex> lock(m_listMutex, std::try_to_lock);
    // if (lock) {
    //   //ListNode* node = hashAccessor->second.m_listNode;
    //   // The list node may be out of the list if it is in the process of being
    //   // inserted or evicted. Doing this check allows us to lock the list for
    //   // shorter periods of time.
    //   if(fast_hash_construct){
    //     ;
    //   } else if (node->isInList()) {
    //     delink(node);
    //     pushFront(node);
    //   }
    //   lock.unlock();
    // }
  }
  if(stat_yes){
    FIFO_FHCache::end_to_end_find_succ++;
  }
  return true;
}

template <class TKey, class TValue, class THash>
bool FIFO_FHCache<TKey, TValue, THash>::find_marker(TValue& ac,
                                                   const TKey& key) {
  HashMapConstAccessor hashAccessor;
  if (!m_map.find(hashAccessor, key)) {
    return false;
  }
  ac = hashAccessor->second.m_value;
  return true;
}

template <class TKey, class TValue, class THash>
bool FIFO_FHCache<TKey, TValue, THash>::get_curve(bool& should_stop) {
  assert(!tier_no_insert.load());
  m_marker = new ListNode(MARKER_KEY);
  size_t pass_counter = 0;

  std::unique_lock<ListMutex> lockA(m_listMutex);
  pushFront(m_marker);
  curve_flag = true;
  FIFO_FHCache::reset_cursor();
  FIFO_FHCache::sample_flag = false;
  lockA.unlock();

  assert(FIFO_FHCache::curve_container.size() == 0);
  size_t FC_size = 0;
  auto start_time = SSDLOGGING_TIME_NOW;
  
  for(int i = 0; i < 45 && !should_stop; i++){
    do{
      usleep(1000);
      double temp_hit = 0, temp_miss = 1;
      FIFO_FHCache::get_step(temp_hit, temp_miss);
      FC_size = movement_counter.load();
      //if(temp_hit + temp_miss > 0.98 && FC_size > m_maxSize * 0.2){
      if(temp_hit + temp_miss > 0.993){ // a magic number; but actually for early stop
        break;
      }
    }while(FC_size <= m_maxSize * i * 1.0/100 * 2 && !should_stop);
    
    printf("\ncurve pass %lu\n", pass_counter++);
    double FC_ratio = FC_size * 1.0 / m_maxSize;
    printf("FC_size: %lu (FC_ratio: %.3lf)\n", FC_size, FC_ratio);
    double FC_hit = 0, miss = 1;
    FIFO_FHCache::print_step(FC_hit, miss);
    auto curr_time = SSDLOGGING_TIME_NOW;
    printf("duration: %.3lf ms\n", SSDLOGGING_TIME_DURATION(start_time, curr_time)/1000);
    start_time = curr_time;
    fflush(stdout);
    if(FC_hit + miss > 0.993 || FC_ratio > 0.9){
      break;
    }
    FIFO_FHCache::curve_container.insert(FIFO_FHCache::curve_container.end(), curve_data_node(FC_ratio, FC_hit, miss));
  }
  FIFO_FHCache::sample_flag = true;

  printf("\ncurve container size: %lu\n", FIFO_FHCache::curve_container.size());
  std::unique_lock<ListMutex> lockB(m_listMutex);
  curve_flag = false;
  delink(m_marker);
  lockB.unlock();
  delete(m_marker);
  m_marker = nullptr;
  movement_counter = 0;

  return true;
}

template <class TKey, class TValue, class THash>
bool FIFO_FHCache<TKey, TValue, THash>::insert(const TKey& key,
                                                     const TValue& value) {
  bool stat_yes = FIFO_FHCache::sample_generator();
  if(stat_yes){
    FIFO_FHCache::insert_count++;
  }

  // Insert into the CHM
  if(tier_no_insert.load())
    return false;
  ListNode* node = new ListNode(key);
  HashMapAccessor hashAccessor;
  HashMapValuePair hashMapValue(key, HashMapValue(value, node));
  if (!m_map.insert(hashAccessor, hashMapValue)) {
    hashAccessor->second = HashMapValue(value, node);
    return false;
  }
  if(fast_hash_construct)
    eviction_counter++;
  // Evict if necessary, now that we know the hashmap insertion was successful.
  size_t s = m_size.load();
  bool evictionDone = false;
  if (s >= m_maxSize) {
    // The container is at (or over) capacity, so eviction needs to be done.
    // Do not decrement m_size, since that would cause other threads to
    // inappropriately omit eviction during their own inserts.
    evictionDone = evict();
  }

  // Note that we have to update the LRU list before we increment m_size, so
  // that other threads don't attempt to evict list items before they even
  // exist.
  std::unique_lock<ListMutex> lock(m_listMutex);
  if(tier_no_insert.load()){
    lock.unlock();
    m_map.erase(hashAccessor);
    delete(node);
    return false;
  }
  if(!curve_flag.load())
    pushFront(node);
  else{
    node->m_time = m_marker->m_time;
    pushAfter(m_marker, node);
  }
  lock.unlock();
  hashAccessor.release(); // for deadlock
  if (!evictionDone) {
    s = m_size++;
  }
  if (s > m_maxSize) {
    // It is possible for the size to temporarily exceed the maximum if there is
    // a heavy insert() load, once only as the cache fills. In this situation,
    // we have to be careful not to have every thread simultaneously attempt to
    // evict the extra entries, since we could end up underfilled. Instead we do
    // a compare-and-exchange to acquire an exclusive right to reduce the size
    // to a particular value.
    //
    // We could continue to evict in a loop, but if there are a lot of threads
    // here at the same time, that could lead to spinning. So we will just evict
    // one extra element per insert() until the overfill is rectified.
    if(evict())
      m_size--;
    // if (m_size.compare_exchange_strong(s, s - 1)) {
    //   evict();
    // }
  }
  return true;
}

template <class TKey, class TValue, class THash>
void FIFO_FHCache<TKey, TValue, THash>::clear() {
  printf("presumably unreachable count ~ %zu\n", unreachable_counter);
  m_map.clear();
  m_fasthash->clear();
  ListNode* node = m_head.m_next;
  ListNode* next;
  while (node != &m_tail) {
    next = node->m_next;
    delete node;
    node = next;
  }
  m_head.m_next = &m_tail;
  m_tail.m_prev = &m_head;

  node = m_fast_head.m_next;
  while (node != &m_fast_tail) {
    next = node->m_next;
    delete node;
    node = next;
  }
  m_fast_head.m_next = &m_fast_tail;
  m_fast_tail.m_prev = &m_fast_head;

  m_size = 0;
}

template <class TKey, class TValue, class THash>
void FIFO_FHCache<TKey, TValue, THash>::snapshotKeys(
    std::vector<TKey>& keys) {
  keys.reserve(keys.size() + m_size.load());
  std::lock_guard<ListMutex> lock(m_listMutex);
  for (ListNode* node = m_head.m_next; node != &m_tail; node = node->m_next) {
    keys.push_back(node->m_key);
  }
}

template <class TKey, class TValue, class THash>
inline void FIFO_FHCache<TKey, TValue, THash>::delink(ListNode* node) {
  assert(node != nullptr);
  ListNode* prev = node->m_prev;
  ListNode* next = node->m_next;
  assert(prev != nullptr);
  prev->m_next = next;
  assert(next != nullptr);
  next->m_prev = prev;
  node->m_prev = OutOfListMarker;
}

template <class TKey, class TValue, class THash>
inline void FIFO_FHCache<TKey, TValue, THash>::pushFront(ListNode* node) {
  ListNode* oldRealHead = m_head.m_next;
  node->m_prev = &m_head;
  node->m_next = oldRealHead;
  oldRealHead->m_prev = node;
  m_head.m_next = node;
}

template <class TKey, class TValue, class THash>
inline void FIFO_FHCache<TKey, TValue, THash>::pushAfter(ListNode* nodeBefore, ListNode* nodeAfter) {
  assert(nodeAfter != nullptr);
  nodeAfter->m_prev = nodeBefore;
  assert(nodeBefore != nullptr);
  nodeAfter->m_next = nodeBefore->m_next;
  nodeBefore->m_next->m_prev = nodeAfter;
  nodeBefore->m_next = nodeAfter;
}

template <class TKey, class TValue, class THash>
bool FIFO_FHCache<TKey, TValue, THash>::evict() {
#ifdef HANDLE_WRITE
  std::unique_lock<ListMutex> lock(m_listMutex);
  ListNode* moribund = m_tail.m_prev;
  while(moribund->m_key == TOMB_KEY) {
    delink(moribund);
    delete moribund;
    moribund = m_tail.m_prev;
    // fast_hash_evict_tomb++;
  }
  if (moribund == &m_head) {
    // List is empty, can't evict
    //printf("List is empty!\n");
    return false;
  }
  delink(moribund);
  lock.unlock();
#else
  std::unique_lock<ListMutex> lock(m_listMutex);
  ListNode* moribund = m_tail.m_prev;
  if (moribund == &m_head) {
    // List is empty, can't evict
    return false;
  }
  // if(!moribund->isInList()){
  //   printf("Error key: %lu\n", moribund->m_key);
  //   debug(0);
  // }
  assert(moribund->isInList());
  delink(moribund);
  lock.unlock();
#endif

  HashMapAccessor hashAccessor;
  if (!m_map.find(hashAccessor, moribund->m_key)) {
    //Presumably unreachable
    //printf("m_key: %ld Presumably unreachable\n", moribund->m_key);
    unreachable_counter++;
    return false;
  }
  m_map.erase(hashAccessor);
  delete moribund;
  return true;
}

// template <class TKey, class TValue, class THash>
// void FIFO_FHCache<TKey, TValue, THash>::evict_key() {
//   evict();
//   m_size--;
// }

#ifdef RECENCY
template <class TKey, class TValue, class THash>
void FIFO_FHCache<TKey, TValue, THash>::delete_key(const TKey& key) {
  // I guess users will not use such keys in find()
  // handle_req_num++;
  // HashMapConstAccessor hashAccessor;
  HashMapAccessor hashAccessor;
  if (!m_map.find(hashAccessor, key)) {
      return;
  }
  TValue ac;
  ListNode* node = hashAccessor->second.m_listNode;
  if(fast_hash_construct) {
    node->m_key = TOMB_KEY;
    if(m_fasthash->find(key, ac)) {
      // TODO: removed
      // m_fasthash.set_key_value(key, nullptr);
      // fast_hash_invalid++;
    }
  }
  else if ((tier_ready || fast_hash_ready) && m_fasthash->find(key, ac) && (ac != nullptr)) {
    node->m_key = TOMB_KEY;
    std::unique_lock<ListMutex> lock(m_listMutex);
    // m_fasthash.set_key_value(key, nullptr);
    // fast_hash_invalid++;
    lock.unlock();
    // fast_hash_try_invalid++;
  }
  else {
    std::unique_lock<ListMutex> lock(m_listMutex);
    //ListNode* node = hashAccessor->second.m_listNode;
    if (!node->isInList()) {
      return;
    }
    delink(node);
    lock.unlock();
    delete node;
  }
  if(m_map.erase(hashAccessor))
    m_size--;
}
#endif

}  // namespace Cache

#endif

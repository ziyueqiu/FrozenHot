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

#pragma once

#define RECENCY

#include <queue>
#include "cache_header.h"

#include <limits>
#include <memory>
// stat
#include <cmath>
#include <set>

namespace tstarling {

  bool should_stop = false;
  //ssdlogging::statistics::MySet request_latency_;
  ssdlogging::statistics::MySet total_hit_latency_;
  ssdlogging::statistics::MySet total_other_latency_;
  ssdlogging::statistics::MySet req_latency_[16];
  std::chrono::_V2::system_clock::time_point cursor;
  size_t print_step_counter = 0;

#ifdef RECENCY
  struct Learning_Result_Node {
    double ratio_, metrics_, granularity_;
    bool status_;
    Learning_Result_Node(double ratio = 0, double metrics = 0, \
      double granularity = 0, bool status = false):
      ratio_(ratio), metrics_(metrics), granularity_(granularity), status_(status) {}
  };

  struct Learning_Input_Node {
    double ratio_, granularity_;
    Learning_Input_Node(double ratio = 0, double granularity = 0):
      ratio_(ratio), granularity_(granularity) {}
  };

  struct cmp {
    bool operator()(Learning_Result_Node a, Learning_Result_Node b) {
      return a.metrics_ > b.metrics_;
    }
  };

  inline bool double_is_equal(double left, double right){
    return (abs(left-right) < 0.0001);
  }
#endif

/**
 * ConcurrentScalableCache is a thread-safe sharded hashtable with limited
 * size. When it is full, it evicts a rough approximation to the least recently
 * used item.
 *
 * The find() operation fills a ConstAccessor object, which is a smart pointer
 * similar to TBB's const_accessor. After eviction, destruction of the value is
 * deferred until all ConstAccessor objects are destroyed.
 *
 * Since the hash value of each key is requested multiple times, you should use
 * a key with a memoized hash function. LRUCacheKey is provided for
 * this purpose.
 */
template <class TKey, class TValue, class THash = tbb::tbb_hash_compare<TKey>>
struct ConcurrentScalableCache {
  using Shard = Cache::CacheAPI<TKey, TValue, THash>;
  enum Type{
    LRU,
    LFU,
    LRU_FH,
    LFU_FH,
    FIFO_FH,
    Redis_LRU,
    FIFO,
    StrictLRU,
  };
  // typedef typename Shard::ConstAccessor ConstAccessor;

  /**
   * Constructor
   *   - maxSize: the maximum number of items in the container
   *   - numShards: the number of child containers. If this is zero, the
   *     "hardware concurrency" will be used (typically the logical processor
   *     count).
   */
  explicit ConcurrentScalableCache(size_t maxSize, size_t numShards = 0, Type type = Type::LRU, int rebuild_freq = 0);

  ConcurrentScalableCache(const ConcurrentScalableCache&) = delete;
  ConcurrentScalableCache& operator=(const ConcurrentScalableCache&) = delete;

  virtual ~ConcurrentScalableCache() {clear(); }

  /**
   * Find a value by key, and return it by filling the ConstAccessor, which
   * can be default-constructed. Returns true if the element was found, false
   * otherwise. Updates the eviction list, making the element the
   * most-recently used.
   */
  //bool find(ConstAccessor& ac, const TKey& key);
  bool find(TValue& ac, const TKey& key);

  /**
   * Insert a value into the container. Both the key and value will be copied.
   * The new element will put into the eviction list as the most-recently
   * used.
   *
   * If there was already an element in the container with the same key, it
   * will not be updated, and false will be returned. Otherwise, true will be
   * returned.
   */
  bool insert(const TKey& key, const TValue& value);

  /**
   * Clear the container. NOT THREAD SAFE -- do not use while other threads
   * are accessing the container.
   */
  void clear();

  void thread_init(int tid) {
    for (size_t i = 0; i < m_numShards; i++) {
        m_shards[i]->thread_init(tid);
    }
  };

  /**
   * Get a snapshot of the keys in the container by copying them into the
   * supplied vector. This will block inserts and prevent LRU updates while it
   * completes. The keys will be inserted in a random order.
   */
  void snapshotKeys(std::vector<TKey>& keys);

  /**
   * Get the approximate size of the container. May be slightly too low when
   * insertion is in progress.
   */
  size_t Size(); 

  virtual void evict_key() {}

  void PrintStat();
  double PrintMissRatio();
  void PrintFrozenStat();
  void PrintGlobalLat();
  double PrintStepLat();
  double PrintStepLat(size_t& total_num);
  double PrintStepLat(double& avg_hit, double& avg_other);
  uint64_t GetStepSize();

  void delete_key(const TKey& key);

  void BaseMonitor();
  void FastHashMonitor();
  void monitor_stop();

  /* a flag to switch on/off the sampling of inner counters*/
  bool stop_sample_stat = true; 

private:
  /**
   * Get the child container for a given key
   */
  Shard& getShard(const TKey& key);

  /**
   * The maximum number of elements in the container.
   */
  size_t m_maxSize;

  /**
   * The child containers
   */
  size_t m_numShards;
  typedef std::shared_ptr<Shard> ShardPtr;
  std::vector<ShardPtr> m_shards;

  Type algType;

  int FROZEN_THRESHOLD; // lifetime factor

#ifdef RECENCY
  int WAIT_STABLE_SLEEP_INTERVAL_US = 500000; // 0.5s check metric per N us
  int CHECK_SLEEP_INTERVAL_US = 100000;
  int WAIT_STABLE_THRESHOLD = 2; // tolerate N times hit ratio volatile

  // learning machine
  double last_sleep = 0;
  std::queue<tstarling::Learning_Input_Node> ratio_container;
  std::priority_queue<tstarling::Learning_Result_Node, std::vector<tstarling::Learning_Result_Node>, cmp> learning_container;
  std::queue<int> construct_container; // Note that this is a queue not a stack
  int PASS_THRESHOLD = 3;

  int COUNT_THRESHOLD = 2;
  double performance_depletion = COUNT_THRESHOLD;
  double best_sleep = 0;
  double best_miss = 1;
  double baseline_performance_for_query = 0;

  int WAIT_DYNAMIC_SLEEP_INTERVAL_US = 10000000; // 10s
  int WAIT_CLEAN_INTERVAL_US = 500000; // 0.5s

  bool beginning_flag = true;
  bool stable_flag = true;

  size_t SLEEP_THRESHOLD = 2; // 2s

  /* a threhold to do conservative choices, 
   * if not outperform too much, don't do FH */
  double FH_PERFORMANCE_THRESHOLD = 0.2;

#endif
};

/**
 * A specialisation of ConcurrentScalableCache providing a cache with efficient
 * string keys.
 */
// template <class TValue>
// using ThreadSafeStringCache = ConcurrentScalableCache<
//     LRUCacheKey, TValue, LRUCacheKey::HashCompare>;

template <class TKey, class TValue, class THash>
ConcurrentScalableCache<TKey, TValue, THash>::
ConcurrentScalableCache(size_t maxSize, size_t numShards, Type type, int rebuild_freq)
  : m_maxSize(maxSize), m_numShards(numShards), algType(type), FROZEN_THRESHOLD(rebuild_freq) 
{
  if (m_numShards == 0) {
    m_numShards = std::thread::hardware_concurrency();
  }
  /* initialize multiple shards (cache instances) */
  for (size_t i = 0; i < m_numShards; i++) {
    size_t s = maxSize / m_numShards;
    size_t t = maxSize % m_numShards;
    if(i < t){
      s = s + 1;
    }
    if(algType == Type::LRU)
      m_shards.emplace_back(std::make_shared<Cache::LRUCache<TKey, TValue, THash>>(s));
    else if(algType == Type::LFU)
      m_shards.emplace_back(std::make_shared<Cache::LFUCache<TKey, TValue, THash>>(s));
    else if(algType == Type::LRU_FH) {
      assert(FROZEN_THRESHOLD > 0);
      m_shards.emplace_back(std::make_shared<Cache::LRU_FHCache<TKey, TValue, THash>>(s));
    }
    else if(algType == Type::Redis_LRU) {
      m_shards.emplace_back(std::make_shared<Cache::RedisLRUCache<TKey, TValue, THash>>(s));
    }
    else if(algType == Type::FIFO) {
      m_shards.emplace_back(std::make_shared<Cache::FIFOCache<TKey, TValue, THash>>(s));
    }
    else if(algType == Type::StrictLRU) {
      m_shards.emplace_back(std::make_shared<Cache::StrictLRUCache<TKey, TValue, THash>>(s));
    }
    else if(algType == Type::LFU_FH) {
      assert(FROZEN_THRESHOLD > 0);
      m_shards.emplace_back(std::make_shared<Cache::LFU_FHCache<TKey, TValue, THash>>(s));
    }
    else if(algType == Type::FIFO_FH) {
      assert(FROZEN_THRESHOLD > 0);
      m_shards.emplace_back(std::make_shared<Cache::FIFO_FHCache<TKey, TValue, THash>>(s));
    }
    else {
      printf("cannot find the cache type\n");
      assert(false);
    }
  }

  // set special parameters for RedisLRU
  if(algType == Type::Redis_LRU) {
    printf("Eviction pool size: %d, sample size: %d\n", EVICTION_POOL_SIZE, EVICTION_SAMPLE);
    printf("Resolution: %.1lf ms\n", LRU_CLOCK_RESOLUTION * 1.0 / 1000);
  }
}

template <class TKey, class TValue, class THash>
typename ConcurrentScalableCache<TKey, TValue, THash>::Shard&
ConcurrentScalableCache<TKey, TValue, THash>::
getShard(const TKey& key) {
  THash hashObj;
  constexpr int shift = std::numeric_limits<size_t>::digits - 16;
  size_t h = (hashObj.hash(key) >> shift) % m_numShards;
  // size_t h = key % m_numShards;
  // std::cout << "before hash: " << hashObj.hash(key) << std::endl;
  // std::cout << "hashed at shard: " << h << std::endl;
  // fflush(stdout);
  return *m_shards.at(h);
}

template <class TKey, class TValue, class THash>
bool ConcurrentScalableCache<TKey, TValue, THash>::
find(TValue & ac, const TKey& key) {
  return getShard(key).find(ac, key);
}

template <class TKey, class TValue, class THash>
bool ConcurrentScalableCache<TKey, TValue, THash>::
insert(const TKey& key, const TValue& value) {
  return getShard(key).insert(key, value);
}

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
delete_key(const TKey& key) {
  getShard(key).delete_key(key);
}

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
clear() {
  // printf("Size: %lu, %.2lf GB\n", size(), size()*4.0/1000000);
  // for (size_t i = 0; i < m_numShards; i++) {
  //   printf("cache No.%ld size %ld\n", i, m_shards[i]->size());
  //   m_shards[i]->clear();
  // }
  // printf("numShards: %lu\n", m_numShards);
}

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
snapshotKeys(std::vector<TKey>& keys) {
  for (size_t i = 0; i < m_numShards; i++) {
    m_shards[i]->snapshotKeys(keys);
  }
}

template <class TKey, class TValue, class THash>
size_t ConcurrentScalableCache<TKey, TValue, THash>::
Size() {
  size_t size = 0;
  for (size_t i = 0; i < m_numShards; i++) {
    size += m_shards[i]->size();
  }
  return size;
}

// template <class TKey, class TValue, class THash>
// void ConcurrentScalableCache<TKey, TValue, THash>::
// evict_key() {
//   size_t temp_size = m_shards[0]->size();
//   size_t temp_i = 0;
//   for (size_t i = 1; i < m_numShards; i++) {
//     if(m_shards[i]->size() > temp_size){
//       temp_i = i;
//       temp_size = m_shards[i]->size();
//     }
//   }
//   m_shards[temp_i]->evict_key();
// }

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
PrintStat(){
  //printf("Size: %lu, %.2lf GB\n", size(), size()*4.0/1000000);
  size_t total_hit = 0, total_miss = 0;
  for (size_t i = 0; i < m_numShards; i++) {
    printf("cache No.%ld size %ld\n", i, m_shards[i]->size());
    size_t hit_num = 0, miss_num = 0;
    m_shards[i]->PrintStat(hit_num, miss_num);
    total_hit += hit_num;
    total_miss += miss_num;
  }
  if(total_hit + total_miss != 0)
    printf("Total hit rate: %.4lf, hit num: %lu, miss num: %lu\n", 
                  total_hit*1.0/(total_hit+total_miss), total_hit, total_miss);
  // else
  //   printf("Total no hit & no miss stat\n");

  printf("numShards: %lu\n", m_numShards);
}

template <class TKey, class TValue, class THash>
double ConcurrentScalableCache<TKey, TValue, THash>::
PrintMissRatio(){
  size_t total_hit = 0, total_miss = 0;
  for (size_t i = 0; i < m_numShards; i++) {
    size_t hit_num = 0, miss_num = 0;
    m_shards[i]->GetStat(hit_num, miss_num);
    total_hit += hit_num;
    total_miss += miss_num;
  }
  double temp = 1;
  if(total_hit + total_miss != 0){
    temp = total_miss*1.0/(total_hit+total_miss);
    printf("Total miss rate: %.4lf, hit num: %lu, miss num: %lu\n", 
                  temp, total_hit, total_miss);
    fflush(stdout);
  }
  return temp;
}

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
PrintFrozenStat(){
  size_t total_FH_hit = 0, total_O_hit = 0, total_miss = 0;
  for (size_t i = 0; i < m_numShards; i++) {
    size_t FH_hit = 0, O_hit = 0, miss = 0;
    m_shards[i]->GetStat(FH_hit, O_hit, miss);
    total_FH_hit += FH_hit;
    total_O_hit += O_hit;
    total_miss += miss;
  }
  double temp, global_miss, total;
  total = total_FH_hit + total_O_hit + total_miss;
  if(total == 0){
      temp = 1;
      global_miss = 1;
  }
  else {
      temp = 1 - total_FH_hit * 1.0/total;
      global_miss = total_miss*1.0/total;
      printf("Total miss rate: %.5f / %.5f, ", temp, global_miss);
      printf("fast find hit: %ld, global hit: %ld, global miss: %ld\n", 
          total_FH_hit, total_O_hit, total_miss);
  }
  //return temp;
}

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
PrintGlobalLat(){
  size_t hit_num = 0, other_num = 0;
  printf("- Hit ");
  auto avg_hit = total_hit_latency_.print_tail(hit_num);
  printf("- Other ");
  auto avg_other = total_other_latency_.print_tail(other_num);
  
  auto total_num = hit_num+other_num;
  printf("Total Avg Lat: %.3lf (size: %lu, miss ratio: %.6lf)\n", 
      (avg_hit*hit_num+avg_other*other_num)/total_num, 
      total_num, other_num*1.0/total_num);
  fflush(stdout);

  cursor = SSDLOGGING_TIME_NOW;
  total_hit_latency_.reset();
  total_other_latency_.reset();
}

template <class TKey, class TValue, class THash>
double ConcurrentScalableCache<TKey, TValue, THash>::
PrintStepLat(){
  size_t hit_num = 0, other_num = 0;
  auto curr_time = SSDLOGGING_TIME_NOW;
  printf("- Hit ");
  auto avg_hit = total_hit_latency_.print_from_last_end(hit_num);
  printf("- Other ");
  auto avg_other = total_other_latency_.print_from_last_end(other_num);
  
  auto total_num = hit_num+other_num;
  auto temp = (avg_hit*hit_num+avg_other*other_num)/total_num;
  printf("Total Avg Lat: %.3lf (size: %lu, duration: %.5lf s, approx miss rate: %.4lf)\n",
    temp, total_num, SSDLOGGING_TIME_DURATION(cursor, curr_time)/1000/1000, other_num*1.0/total_num);
  cursor = curr_time;
  return temp;
}

template <class TKey, class TValue, class THash>
uint64_t ConcurrentScalableCache<TKey, TValue, THash>::
GetStepSize(){
  auto hit_num = total_hit_latency_.size_from_last_end();
  auto miss_num = total_other_latency_.size_from_last_end();
  return hit_num+miss_num;
}

template <class TKey, class TValue, class THash>
double ConcurrentScalableCache<TKey, TValue, THash>::
PrintStepLat(size_t& total_num){
  size_t hit_num = 0, other_num = 0;
  auto curr_time = SSDLOGGING_TIME_NOW;
  printf("- Hit ");
  auto avg_hit = total_hit_latency_.print_from_last_end(hit_num);
  printf("- Other ");
  auto avg_other = total_other_latency_.print_from_last_end(other_num);
  
  total_num = hit_num+other_num;
  auto temp = (avg_hit*hit_num+avg_other*other_num)/total_num;
  printf("Total Avg Lat: %.3lf (size: %lu, duration: %.5lf s, approx miss rate: %.4lf)\n",
    temp, total_num, SSDLOGGING_TIME_DURATION(cursor, curr_time)/1000/1000, other_num*1.0/total_num);
  cursor = curr_time;
  return temp;
}

template <class TKey, class TValue, class THash>
double ConcurrentScalableCache<TKey, TValue, THash>::
PrintStepLat(double& avg_hit, double& avg_other){
  size_t hit_num = 0, other_num = 0;
  auto curr_time = SSDLOGGING_TIME_NOW;
  printf("- Hit ");
  avg_hit = total_hit_latency_.print_from_last_end(hit_num);
  printf("- Other ");
  avg_other = total_other_latency_.print_from_last_end(other_num);
  
  auto total_num = hit_num+other_num;
  auto temp = (avg_hit*hit_num+avg_other*other_num)/total_num;
  printf("Total Avg Lat: %.3lf (size: %lu, duration: %.5lf s, approx miss rate: %.4lf)\n",
    temp, total_num, SSDLOGGING_TIME_DURATION(cursor, curr_time)/1000/1000, other_num*1.0/total_num);
  cursor = curr_time;
  return temp;
}

#ifdef RECENCY
// double Median(double a[], int N) {
//   int i,j,max;
//   double t;
//   for(i=0;i<N-1;i++)
//   {
//       max=i;
//       for(j=i+1;j<N;j++)
//         if(a[j]>a[max]) max=j;
//       t=a[i];
//       a[i]=a[max];
//       a[max]=t;
//   }
//   return a[(N-1)/2];
// }

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
BaseMonitor(){
  printf("wait stable interval: %d us (%.3lf s)\n",
      WAIT_STABLE_SLEEP_INTERVAL_US, WAIT_STABLE_SLEEP_INTERVAL_US*1.0/1000/1000);
  //printf("wait stable interval: 1s\n");

  auto start_wait_stable = SSDLOGGING_TIME_NOW;
  double last_miss_ratio = 1.5;
  double miss_ratio;
  size_t last_size = 0, size = 0;
  int wait_count = 0;
  //size_t print_step_counter = 0;

  /* warm up run (non-valid) */
  while(!should_stop){
    printf("\ndata pass %lu\n", print_step_counter++);
    miss_ratio = PrintMissRatio();
    PrintStepLat();
    if(last_size >= size){
      if(last_miss_ratio <= miss_ratio)
        wait_count++;
      if(wait_count >= WAIT_STABLE_THRESHOLD){
        printf("- miss ratio = %.5lf -> %.5lf, with m_size = %lu (max = %lu)\n",
                  last_miss_ratio, miss_ratio, size, m_maxSize);
        break;
      }
    }
    last_size = size;
    size = Size();
    printf("- miss ratio = %.5lf -> %.5lf, with m_size = %lu (max = %lu)\n",
                  last_miss_ratio, miss_ratio, size, m_maxSize);
    fflush(stdout);
    last_miss_ratio = miss_ratio;
    usleep(WAIT_STABLE_SLEEP_INTERVAL_US);
    //sleep(1);
  }
  printf("\nthe first wait stable\n");
  printf("clear stat for next stage:\n");
  PrintGlobalLat(); // with clear inside

  auto wait_stable_duration = SSDLOGGING_TIME_DURATION(start_wait_stable, SSDLOGGING_TIME_NOW);
  printf("\nwait stable spend time: %lf s\n", wait_stable_duration / 1000 / 1000);

  /* valid run */
  while(!should_stop){
    printf("\ndata pass %lu\n", print_step_counter++);
    //usleep(WAIT_STABLE_SLEEP_INTERVAL_US);
    sleep(1);
    PrintMissRatio();
    PrintStepLat();
  }
  return;
}

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
FastHashMonitor(){
  printf("start monitor...\n");
  printf("wait stable interval: %d us (%.1lf s)\n",
      WAIT_STABLE_SLEEP_INTERVAL_US, WAIT_STABLE_SLEEP_INTERVAL_US*1.0/1000/1000);
  
  printf("Add extra FH performance threshold %.2lf for construction & frozen\n",
      FH_PERFORMANCE_THRESHOLD);

  // use shard 0 simulate all the cache
  // TODO @ Ziyue: could be independent, not only shard 0
WAIT_STABLE:
  auto start_wait_stable = SSDLOGGING_TIME_NOW;
  double last_miss_ratio = 1.5;
  double miss_ratio;

  /* latency in this setting
   * not throughput
   */
  double performance;
  size_t thput_step;
  size_t last_size = 0, size = 0;
  stop_sample_stat = true;
  int wait_count = 0;
  double monitor_time = 0;

  // Fill up cache
  printf("\n* start observation *\n");
  while(!should_stop) {
    // Note that 'should_stop' is set true when clients run out of the workload
    // It is a user-level signal, not a signal from the monitor
    printf("\ndata pass %lu\n", print_step_counter++);
    miss_ratio = PrintMissRatio();
    PrintStepLat();
    
    /* This logic is for the case that the cache is already full != cache is stable
     * still need several extra loops to evict the coldest items
     * the metric is miss ratio non-decreasing */
    if(last_size >= size){ // cache is full
      if(last_miss_ratio <= miss_ratio) // Miss ratio is non-decreasing
        wait_count++;
      if(wait_count >= WAIT_STABLE_THRESHOLD){ // Waiting too long, stop filling process
        printf("- miss ratio = %.5lf -> %.5lf, with m_size = %lu (max = %lu)\n",
                  last_miss_ratio, miss_ratio, size, m_maxSize);
        break;
      }
    }
    
    // Update size
    last_size = size;
    size = Size();
    printf("- miss ratio = %.5lf -> %.5lf, with m_size = %lu (max = %lu)\n",
                  last_miss_ratio, miss_ratio, size, m_maxSize);
    fflush(stdout);
    last_miss_ratio = miss_ratio; // Update miss ratio
    usleep(WAIT_STABLE_SLEEP_INTERVAL_US);
  }
  printf("\ncache is stable\n");
  
  // Message for end filling
  if(beginning_flag){
    printf("the first wait stable\n");
    printf("clear stat for next stage:\n");
    PrintGlobalLat();
    beginning_flag = false;
  }
  auto wait_stable_duration = SSDLOGGING_TIME_DURATION(start_wait_stable, SSDLOGGING_TIME_NOW);
  printf("\nwait stable spend time: %lf s\n", wait_stable_duration / 1000 / 1000);

  monitor_time = double(500 * m_numShards);

  printf("\n* end observation *\n");

  printf("\n* start search *\n");
  auto start_learning_time = SSDLOGGING_TIME_NOW;

  // get curve
  if(!should_stop) // get FC hit, DC hit, DC miss each FC_ratio
    m_shards[0]->get_curve(should_stop);

  // calculate optimal FC_ratio
  auto container = m_shards[0]->curve_container;
  double best_avg = 1000, best_size = 0;
  double DC_hit_lat = 0, miss_lat = 0, FC_lat = 0;
  double disk_lat = 0;
  double Frozen_Avg = 0, Frozen_Miss = 0, best_option_tMiss = 0;

  /* observe baseline for a while */
  do{
    usleep(WAIT_STABLE_SLEEP_INTERVAL_US);
  }while(total_other_latency_.size_from_last_end() < 5 && !should_stop);
  printf("\ndraw curve\ndata pass %lu\n", print_step_counter++);
  PrintMissRatio();
  /* become aware of DC latency (hit lat) and DC miss latency + disk latency (miss lat) */
  PrintStepLat(DC_hit_lat, miss_lat);

  // first do 100% FC
  for(size_t i = 0; i < m_numShards; i++){
    m_shards[i]->construct_tier();
  }
  printf("\ndata pass %lu\n", print_step_counter++);
  PrintMissRatio();
  PrintStepLat();

  /* Controller "sleep" for a while
   * the cache runs in 100% FH, 
   * get performance and miss ratio after sleep
   */
  usleep(WAIT_STABLE_SLEEP_INTERVAL_US);
  printf("\ndata pass %lu\n", print_step_counter++);
  
  /* get one endpoint of FC miss ratio in curve -- when 100% FC
   * but not used (only printed)
   */
  Frozen_Miss = PrintMissRatio();
  /* become aware of FC latency (hit lat) and disk latency (miss lat) */
  Frozen_Avg = PrintStepLat(FC_lat, disk_lat);
  for(size_t i = 0; i < m_numShards; i++){
    m_shards[i]->deconstruct();
  }

  printf("\nHothash lat: %.3lf, 100%% Frozen Avg lat: %.3lf us, Miss Rate: %.3lf\n",
    FC_lat, Frozen_Avg, Frozen_Miss);
  fflush(stdout);

  // in-cache hit miss is only lookup pass metrics, e.g. 99% hit
  // for client, hit may be 50%, miss ~0.5%, then 49.5% write (i.e. insert)
  // better add a counter for insert()

  // Calculate the best FC ratio for shard 0, but will be applied to all shards
  for(size_t i = 0; i < container.size(); i++){
    auto tSize = container[i].size_;
    auto tFC_hit_ = container[i].FC_hit_;
    auto tMiss = container[i].miss_;
    double avg;
    if(tSize < 0.01){
      avg = tMiss * miss_lat + (1-tMiss) * DC_hit_lat;
      tSize = 0;
      printf("when baseline, avg from %.3lf to %.3lf\n",
          avg, avg / (1 + FH_PERFORMANCE_THRESHOLD));
      avg = avg / (1 + FH_PERFORMANCE_THRESHOLD);
    }

    // when tSize is large, we regard it as ~100% FC
    else if(i == container.size() - 1 && tSize > 0.65){
      //avg = tMiss * disk_lat + (1-tMiss) * FC_lat;
      printf("regard tSize from %.3lf to %d\n", tSize, 1);
      tSize = 1;
      avg = tFC_hit_ * FC_lat + tMiss * (miss_lat+FC_lat) + (1-tFC_hit_-tMiss) * (FC_lat + DC_hit_lat);
    }
    else {
      avg = tFC_hit_ * FC_lat + tMiss * (miss_lat+FC_lat) + (1-tFC_hit_-tMiss) * (FC_lat + DC_hit_lat);
    }

    if(avg < best_avg){
      best_avg = avg;
      best_size = tSize;
      best_option_tMiss = tMiss;
      /* only print when best avg is updated */
      printf("(Update) best avg: %.3lf us, best size: %.3lf (w. FC_hit: %.3lf, miss: %.3lf)\n",
        best_avg, best_size, tFC_hit_, best_option_tMiss);
    }
  }
  
  // Compare best "partially" frozen [0, 100%) one with 100% Frozen
  if(best_avg > Frozen_Avg){
    printf("(Update) best avg: %.3lf us, best size: %.3lf\n",
      Frozen_Avg, 1.0);
    best_avg = Frozen_Avg;
    best_size = 1.0;
  }
  
  // clear curve container
  // Only using shard 0 for profiling
  m_shards[0]->curve_container.clear();

  auto learning_duration = SSDLOGGING_TIME_DURATION(start_learning_time, SSDLOGGING_TIME_NOW);
  printf("\nsearch time: %lf s\n", learning_duration / 1000 / 1000);
  printf("Profiling best size: %.3lf\n", best_size);

  // debugging
  // printf("\nDebug: Test 1.0 Size\n");
  // best_size = 1;
  
  // End of FH evaluation, and got the desired ratio to construct
  printf("\n* end search *\n");
  
  if(!should_stop && best_size < 0.05){ // 0.05 is magic
    // not suitable for FH
    // sleep longer and longer (wait)
    SLEEP_THRESHOLD *= 8; // 8 is magic
    printf("Sleep threshold increase to %zu\n", SLEEP_THRESHOLD);
    printf("Sleep %zu s\n", SLEEP_THRESHOLD);
    fflush(stdout);
    for(size_t i = 0; i < SLEEP_THRESHOLD; i++){
      if(should_stop)
        break;
      sleep(1);
      printf("\ndata pass %lu\n", print_step_counter++);
      PrintMissRatio();
      PrintStepLat();
    }
    if(!should_stop){
      goto WAIT_STABLE;
    }
  }

  // Construct the FH
CONSTRUCT:
  auto start_construct_time = SSDLOGGING_TIME_NOW;
  double baseline_metrics[80];
  std::set<int> fail_list;
  fail_list.clear();
  printf("\n* start construct *\n");

  assert(construct_container.empty());

  printf("\nFind median avg lat of baseline:");
  do{
    sleep(1);
  }while(GetStepSize() < 100 && !should_stop);
  printf("\ndata pass %lu\n", print_step_counter++);
  PrintMissRatio();

  size_t baseline_step = 0;
  size_t construct_step = 0;
  
  baseline_performance_for_query = PrintStepLat(construct_step);
  double baseline_performance_with_threshold = baseline_performance_for_query  / (1 + FH_PERFORMANCE_THRESHOLD);

  printf("FH compare with baseline metric: %.3lf\n", baseline_performance_for_query);
  printf("FH compare with baseline metric with threshold: %.3lf\n", baseline_performance_with_threshold);

  stop_sample_stat = false; // enable req_latency_[]

  /* req_latency_[] array is used to store the latency of each shard */
  printf("\nPer shard baseline print tail\n");
  for(size_t i = 0; i < m_numShards; i++) { // initialization
    req_latency_[i].reset();
  }
  usleep(WAIT_STABLE_SLEEP_INTERVAL_US);
  for(size_t i = 0; i < m_numShards; i++) { // initialization
    construct_container.push(i);
    baseline_metrics[i] = req_latency_[i].print_tail();
  }

  int pass_count = 0;

  /*
   * 1. construct_container is used to store the shard id that we want to construct FH
   * 2. fail_list is used to store the shard id that we failed to construct FH
   *    The reason to have fail_list is that we don't have to construct FH for all shards
   *    We only construct FH for shards that can EASILY see improvement (i.e. see within a few try)
   *    TODO @ Ziyue: do sensitivity analysis to explore more about this threshold
   * 3. pass_count is used to count the number of pass
   * example: we have 4 shards, and we want to construct FH for shard 0, 1, 2, 3, allow 2 pass
   *         Init: construct_container = {0, 1, 2, 3} fail_list = {} pass_count = 0 < 2
   *         1st pass: 0~2 success, 3 fail, construct_container = {3} fail_list = {} pass_count = 1
   *         2nd pass: 3 success, construct_container = {} fail_list = {} pass_count = 2 end;
   *         or 2nd pass: 3 fail, construct_container = {} fail_list = {3} pass_count = 2 end;
   */
  while(!should_stop && !construct_container.empty()) {
    size_t pass_len = construct_container.size();
    for(size_t k = 0; k < pass_len; k++){
      // simply print with order
      // TODO @ Ziyue: maybe we can use a better way to print (just indexing?)
      printf("%d ", construct_container.front());
      construct_container.push(construct_container.front());
      construct_container.pop();
    }
    printf("left\n");

    /* alloc maximum = PASS_THRESHOLD times of pass */
    if(pass_count >= PASS_THRESHOLD){
      while(!construct_container.empty()){
        int shard_id = construct_container.front();
        construct_container.pop();
        fail_list.insert(shard_id);
      }
      break;
    }
    
    // Call the construct function for each shard
    for(size_t i = 0; i < pass_len; i++) {
      size_t shard_id = construct_container.front();
      if(double_is_equal(best_size, 1))
        m_shards[shard_id]->construct_tier();
      else
        m_shards[shard_id]->construct_ratio(best_size);
      
      m_shards[shard_id]->reset_cursor();
      
      // just a scan
      construct_container.pop();
      construct_container.push(shard_id);
    }

    for(size_t i = 0; i < pass_len; i++) { // clear req_latency_[] array
      size_t shard_id = construct_container.front();
      req_latency_[shard_id].reset();
      construct_container.pop();
      construct_container.push(shard_id);
    }
    // query
    printf("try query\n");
    fflush(stdout);
    usleep(monitor_time);
    
    // Check if some shards are not suitable for FH and deconstruct
    for(size_t i = 0; i < pass_len; i++) {
      size_t shard_id = construct_container.front();
      m_shards[shard_id]->print_step();
      performance = req_latency_[shard_id].print_tail();
      construct_container.pop();
      // if the latency is larger than the baseline plus threshold, then deconstruct (fail)
      if(performance > baseline_metrics[shard_id] / (1 + FH_PERFORMANCE_THRESHOLD)) {
        construct_container.push(shard_id);
        m_shards[shard_id]->deconstruct();

        // clear baseline latency, since not baseline during FH phase
        req_latency_[shard_id].reset();
        usleep(monitor_time);
        printf("shard %lu baseline metrics (update):\n", shard_id);
        baseline_metrics[shard_id] = req_latency_[shard_id].print_tail();
      }
    }
    printf("pass %d end\n", pass_count++);
    fflush(stdout);

    printf("\nconstruct phase:\ndata pass %lu\n", print_step_counter++);
    PrintMissRatio();
    size_t temp_step = 0;
    PrintStepLat(temp_step);
    construct_step += temp_step;
  }

  printf("\nconstruct step: %lu\n", construct_step);

  // Means all shards fail, pretty bad :(
  if(!should_stop && fail_list.size() == m_numShards){
    // TODO @ Ziyue: goto observation phase or search phase?
    goto WAIT_STABLE;
  }

  // Message for construction phase details
  auto construct_duration = SSDLOGGING_TIME_DURATION(start_construct_time, SSDLOGGING_TIME_NOW);
  printf("\nconstruct time: %lf s\n", construct_duration / 1000 / 1000);
  if(fail_list.size() == 0)
    printf("fail %ld shard, construct fully succeed\n", fail_list.size());
  else
    printf("fail %ld shard\n", fail_list.size());
  
  printf("\n* end construct *\n"); // End of the FH construction

  stop_sample_stat = true; // disable req_latency_[] in client logic
                           // this helps us do less job
  
  // Start of Frozen run after construction, and monitor it
  auto start_query_stage = SSDLOGGING_TIME_NOW;
  printf("\n* start frozen *\n");
  
  double check_time = CHECK_SLEEP_INTERVAL_US;
  printf("check interval: %.3lf s\n", check_time/1000/1000);
  performance_depletion = COUNT_THRESHOLD;
  bool first_flag = true;
  size_t total_step = 0;
  size_t current_step = 0;
  
  while(!should_stop) {
    do{
      usleep(check_time);
    } while(GetStepSize() < 50 && !should_stop);
    
    printf("\ndata pass %lu\n", print_step_counter++);
    PrintFrozenStat();
    performance = PrintStepLat(thput_step);
    if(first_flag){
      baseline_step = thput_step;
      first_flag = false;
    }
    
    // TODO @ Ziyue: window-based will be better?
    // TODO @ Ziyue: baseline shall be refreshed periodically?
    // TODO @ Ziyue: how to become 'optimal' instead of only better than baseline
    auto delta = (baseline_performance_with_threshold - performance)/baseline_performance_with_threshold * thput_step / baseline_step;
    performance_depletion += delta; // INTEGRATION to help decide whether to stop FH
    
    // Decide whether the FH performance is too bad, and go to baseline
    if(performance_depletion <= 0){
    // give 3 times baseline as start-up capital,
    // if depleted, goto DECONSTRUCT
      printf("depleted: %.3lf <= 0\n\n", performance_depletion);
      SLEEP_THRESHOLD *= 8;
      printf("Sleep threshold increase to %zu\n", SLEEP_THRESHOLD);
      goto DECONSTRUCT;
    }
    else{
      printf("not depleted: %.3lf > 0\n", performance_depletion);
    }

    total_step += thput_step;
    current_step += thput_step;
    if(total_step > construct_step * FROZEN_THRESHOLD){ // Periodic Reconstruction of FH
      printf("\nAfter %lu frozen step (> %d * %lu = %lu), need periodically refresh!\n",
        total_step, FROZEN_THRESHOLD, construct_step, construct_step * FROZEN_THRESHOLD);
      for(size_t i = 0; i < m_numShards; i++) {
        m_shards[i]->deconstruct();
      }
      // TODO @ Ziyue: wait stable for some time?
      sleep(1); // 1 sec for test

      if(SLEEP_THRESHOLD >= 2)
        SLEEP_THRESHOLD /= 2;
      printf("Perform Well, Sleep Threshold decrease into %zu\n", SLEEP_THRESHOLD);
      goto CONSTRUCT;
    } else if(current_step > construct_step) {
      /* this is a round to check whether need to deconstruct
       * since performance_depletion is initialized to COUNT_THRESHOLD,
       * if performance_depletion > COUNT_THRESHOLD, it means FH still outperforms baseline
       * so we can continue to run FH, but need to reset performance_depletion to COUNT_THRESHOLD
       * or we will not be able to beware of the performance degradation till deplete all benefits
       * 
       * else (performance_depletion <= COUNT_THRESHOLD), it means FH is not good enough
       */
      if(performance_depletion > COUNT_THRESHOLD){
        printf("\nPeformance Depletion set to %d after %zu step for a round!\n", COUNT_THRESHOLD, current_step);
        // it is still good enough
        performance_depletion = COUNT_THRESHOLD;
        current_step = 0;
      } else {
        // it is not good enough, need to deconstruct
        printf("\nAfter %lu frozen step (~ %d * %lu), need to refresh!\n",
          total_step, int(total_step/construct_step),construct_step);
        for(size_t i = 0; i < m_numShards; i++) {
          m_shards[i]->deconstruct();
        }
        sleep(1); // 1 sec for test
        goto CONSTRUCT;
      }
    }
    fflush(stdout);
  }
  
  // Deconstruction
DECONSTRUCT:
  for(size_t i = 0; i < m_numShards; i++) {
    m_shards[i]->deconstruct();
  }

  auto query_duration = SSDLOGGING_TIME_DURATION(start_query_stage, SSDLOGGING_TIME_NOW);
  printf("\nfrozen stage duration time: %lf\n", query_duration /1000 / 1000);
  printf("\n* end frozen *\n");
  if(!should_stop){
    printf("Sleep %zu s\n", SLEEP_THRESHOLD);
    fflush(stdout);
    for(size_t i = 0; i < SLEEP_THRESHOLD; i++){
      sleep(1);
      printf("\ndata pass %lu\n", print_step_counter++);
      PrintMissRatio();
      PrintStepLat();
      if(should_stop)
        break;
    }
  }
  
  /* Q1: Why wait stable instead of immediately constructing?
   * Q2: Isn't wait stable for cache warm up, and we already run the cache for so long?
   * WAIT_STABLE is not just for warmup, can also be a recovery from FH
   * for example, after deconstruct, we need to wait for a while to let the cache
   * to recover itself, and then we can start to construct
   * or the profiling results (curve) might not be accurate
   */
  if(!should_stop){
    printf("\ngo back to wait stable\n");
    goto WAIT_STABLE;
  }
  printf("\nend monitor\n");
}

template <class TKey, class TValue, class THash>
void ConcurrentScalableCache<TKey, TValue, THash>::
monitor_stop(){
  should_stop = true;
  printf("\nshould stop = true\n");
}
#endif
} // namespace tstarling

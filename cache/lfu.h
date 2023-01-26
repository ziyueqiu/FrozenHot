#include <unordered_map>
#include <tbb/concurrent_hash_map.h>
#include <iostream>
#include <map>
#include <list>
#include <shared_mutex>

#include "baseCache.h"

namespace Cache {
template <class TKey, class TValue, class THash = tbb::tbb_hash_compare<TKey>>
class LFUCache : public BaseCacheAPI<TKey, TValue, THash>{
    using BaseCache = Cache::CacheAPI<TKey, TValue, THash>;
    struct freListNode {
        freListNode() : m_key(0), m_prev(nullptr), m_next(nullptr) {}

        freListNode(const TKey& key)
            : m_key(key), m_prev(nullptr), m_next(nullptr) {}

        TKey m_key;
        freListNode* m_prev;
        freListNode* m_next;

        bool isInList() const { return m_prev != nullptr; }
    };

    struct ListNode {
        ListNode() : fre(0), m_prev(nullptr), m_next(nullptr) {}

        ListNode(const int& f)
            : fre(f), m_prev(nullptr), m_next(nullptr) {
            list.m_next = &list;
            list.m_prev = &list;
        }

        void pushFront(freListNode* node){
            freListNode* oldRealHead = list.m_next;
            node->m_prev = &list;
            node->m_next = oldRealHead;
            oldRealHead->m_prev = node;
            list.m_next = node;
        }

        bool isEmpty() { return (list.m_next == &list) && (list.m_prev == &list);}

        long long int fre;
        freListNode list;
        ListNode* m_prev;
        ListNode* m_next;

        //bool isInList() const { return m_prev != nullptr; }
    };
    //static ListNode* const OutOfListMarker;

    struct HashMapValue{
        HashMapValue() : listNode(nullptr), freNode(nullptr) {}
        HashMapValue(const TValue& v, ListNode* node, freListNode* it)
            : val(v), listNode(node), freNode(it) {}
        TValue val;
        ListNode* listNode;
        freListNode* freNode;
    };

    typedef tbb::concurrent_hash_map<TKey, HashMapValue, THash> HashMap;
    typedef typename HashMap::const_accessor HashMapConstAccessor;
    typedef typename HashMap::accessor HashMapAccessor;
    typedef typename HashMap::value_type HashMapValuePair;

private:
    size_t n; // max num
    std::atomic<size_t> s;

    HashMap m_map;

    typedef std::mutex ListMutex;
    ListMutex gLock;
    ListNode m_head;
    ListNode m_tail;

#ifdef FH_DESIGN
    std::unique_ptr<SPTAG::FashHashAPI<TValue>>  m_fasthash;
    bool FH_ready = false;
    bool FH_construct = false;

    ListNode m_fast_head;
    ListNode m_fast_tail;
    ListMutex m_fast_listMutex;
#endif
    
public:
    //typename LFUCache::ListNode* const OutOfListMarker = (ListNode*)-1;
    LFUCache(size_t capacity) {
        n = capacity;
        s = 0;

        m_head.m_prev = nullptr;
        m_head.m_next = &m_tail;
        m_tail.m_prev = &m_head;

#ifdef FH_DESIGN
        m_fast_head.m_prev = nullptr;
        m_fast_head.m_next = &m_fast_tail;
        m_fast_tail.m_prev = &m_fast_head;

        m_fasthash.reset(new SPTAG::OptHashPosVector<TValue>(16384, 1));
#endif

        LFUCache::reset_stat();
    }
    virtual ~LFUCache() { clear(); }

    virtual size_t size() const override { return s.load(); }

    virtual void clear() override {
        if(m_head.m_next != &m_tail)
            printf("max fre: %lld\n", m_head.m_next->fre);
        m_map.clear();
        ListNode* node = m_head.m_next;
        ListNode* next;
        while (node != &m_tail) {
            next = node->m_next;
            delete node;
            node = next;
        }
        m_head.m_next = &m_tail;
        m_tail.m_prev = &m_head;

        s = 0;
    }
    // void thread_init(int tid) {
    //     m_fasthash->thread_init(tid);
    // }

    template<typename NodeType>
    inline void delink(NodeType* node) {
        assert(node != nullptr);
        NodeType* prev = node->m_prev;
        NodeType* next = node->m_next;
        assert(prev != nullptr);
        prev->m_next = next;
        assert(next != nullptr);
        next->m_prev = prev;
        //node->m_prev = OutOfListMarker;
        node->m_prev = nullptr;
        node->m_next = nullptr;
    }

    // inline void pushTail(ListNode* node) {
    //     //if(tier_no_insert) return;
    //     ListNode* oldRealTail = m_tail.m_prev;
    //     node->m_next = &m_tail;
    //     node->m_prev = oldRealTail;
    //     oldRealTail->m_next = node;
    //     m_tail.m_prev = node;
    // }
    
    inline void pushForward(ListNode* node, ListNode* new_node){
        assert(node != &m_head);
        assert(new_node->fre > node->fre);
        new_node->m_prev = node->m_prev;
        new_node->m_next = node;
        node->m_prev->m_next = new_node;
        node->m_prev = new_node;
    }

    virtual bool find(TValue& ac, const TKey& key) override {
        //HashMapConstAccessor hashAccessor;
        HashMapAccessor hashAccessor; // TODO: replace to const and use replace
        if(!m_map.find(hashAccessor, key)) {
            if(LFUCache::sample_generator())
                LFUCache::miss_num++;
            return false;
        }
        ac = hashAccessor->second.val;
        //std::unique_lock<ListMutex> lock(gLock, std::try_to_lock);
        std::unique_lock<ListMutex> lock(gLock);

        freListNode* frenode = hashAccessor->second.freNode;
        ListNode* listnode = hashAccessor->second.listNode;
        //assert(listnode != nullptr);
        if(listnode == nullptr){ // TODO
            printf("key: %ld\n", key);
            assert(false);
        }
        auto fre = listnode->fre;
        if(frenode->isInList()){
            //assert(listnode->m_prev->fre > fre);
            if(listnode->m_prev == &m_head || listnode->m_prev->fre > fre+1){
                if(frenode->m_prev == frenode->m_next){ // empty
                    assert(frenode != &listnode->list);
                    listnode->fre++;
                }
                else{
                    delink<freListNode>(frenode);
                    ListNode* newnode = new ListNode(fre+1);
                    newnode->pushFront(frenode);
                    pushForward(listnode, newnode);
                    hashAccessor->second.listNode = newnode;
                }
            } else if(listnode->m_prev->fre <= fre){
                printf("prev fre: %lld <= fre: %lld\n", listnode->m_prev->fre, fre);
                assert(false);
            } else {
                delink<freListNode>(frenode);
                hashAccessor->second.listNode = listnode->m_prev;
                listnode->m_prev->pushFront(frenode);
                if(listnode->isEmpty()){
                    delink<ListNode>(listnode);
                    delete(listnode);
                }
            }
        }
        lock.unlock();
        if(LFUCache::sample_generator())
            LFUCache::hit_num++;
        return true;
    }
    
    virtual bool insert(const TKey& key, const TValue& value) override {
        freListNode* node = new freListNode(key);
        HashMapAccessor hashAccessor;
        HashMapValuePair hashMapValue(key, HashMapValue(value, nullptr, node));
        // if nullptr, promotion fail
        if (!m_map.insert(hashAccessor, hashMapValue)) {
            delete node;
            //fail_num++;
            //hashAccessor->second = HashMapValue(value, nullptr, node); // why?
            return false;
        }
        std::unique_lock<ListMutex> lock(gLock);
        ListNode* lfreNode = m_tail.m_prev;
        //printf("insert key: %ld, test listnode fre: %ld\n", key, lfreNode->fre);
        if(lfreNode->fre == 1){
            lfreNode->pushFront(node);
        } else {
            lfreNode = new ListNode(1);
            lfreNode->pushFront(node);
            //pushTail(lfreNode);
            pushForward(&m_tail, lfreNode);
        }
        hashAccessor->second.listNode = lfreNode;
        hashAccessor.release();
        // debug
        // {
        //     m_map.find(hashAccessor, key);
        //     printf("success? %d\n", hashAccessor->second.listNode != nullptr);
        // }
        

        size_t size = s.load();
        assert(size <= n);
        if(size < n){
            s++;
        }
        else { // evict
            ListNode* moribund = m_tail.m_prev;
            assert(moribund != &m_head);
            freListNode* temp_node = moribund->list.m_prev;
            assert(temp_node != &moribund->list);

            delink<freListNode>(temp_node);
            //printf("evict key: %ld, with fre: %ld\n", temp_node->m_key, moribund->fre);
            lock.unlock();

            HashMapAccessor hashAccessor;
            if (!m_map.find(hashAccessor, temp_node->m_key)) {
                //fail_num++;
                return false;
            }
            m_map.erase(hashAccessor);
            delete temp_node;
        }
        //success_num++;
        return true;
    }
};

/**
 * Your LFUCache object will be instantiated and called as such:
 * LFUCache* obj = new LFUCache(capacity);
 * int param_1 = obj->find(key);
 * obj->insert(key,value);
 */
} // namespace
#include <unordered_map>
#include <tbb/concurrent_hash_map.h>
#include <iostream>
#include <map>
#include <list>
#include <shared_mutex>

#include "FHCache.h"

namespace Cache {
template <class TKey, class TValue, class THash = tbb::tbb_hash_compare<TKey>>
class LFU_FHCache : public FHCacheAPI<TKey, TValue, THash>{
    //using BaseCache = Cache::CacheAPI<TKey, TValue, THash>;
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
        //typename std::list<TKey>::iterator item;
    };

    typedef tbb::concurrent_hash_map<TKey, HashMapValue, THash> HashMap;
    typedef typename HashMap::const_accessor HashMapConstAccessor;
    typedef typename HashMap::accessor HashMapAccessor;
    typedef typename HashMap::value_type HashMapValuePair;

private:
    size_t m_maxSize; // max num
    std::atomic<size_t> s;

    HashMap m_map;

    typedef std::mutex ListMutex;
    ListMutex m_listMutex;
    ListNode m_head;
    ListNode m_tail;

    std::unique_ptr<FastHash::FashHashAPI<TValue>>  m_fasthash;
    bool FH_ready = false;
    bool FH_construct = false;
    bool tier_ready = false;
    std::atomic<bool> tier_no_insert{false};
    std::atomic<bool> curve_flag{false};

    ListNode m_fast_head;
    ListNode m_fast_tail;
    int fail_num = 0;
    long long int m_threshold = 0;
    
public:
    //typename LFU_FHCache::ListNode* const OutOfListMarker = (ListNode*)-1;
    
    LFU_FHCache(size_t capacity) {
        m_maxSize = capacity;
        s = 0;

        m_head.m_prev = nullptr;
        m_head.m_next = &m_tail;
        m_tail.m_prev = &m_head;

        m_fast_head.m_prev = nullptr;
        m_fast_head.m_next = &m_fast_tail;
        m_fast_tail.m_prev = &m_fast_head;

        int align_len = 1 + int(log2(m_maxSize));
        m_fasthash.reset(new FastHash::CLHT<TValue>(0, align_len));
        //m_fasthash.reset(new FastHash::TbbCHT<TValue>(m_maxSize));

        //LFU_FHCache::reset_stat();
    }
    virtual ~LFU_FHCache() { clear(); }

    void thread_init(int tid) override {
        m_fasthash->thread_init(tid);
    }

    virtual size_t size() const override { return s.load(); }

    virtual bool is_full() override { return s.load() >= m_maxSize; }

    virtual void clear() override {
        if(m_head.m_next != &m_tail)
            printf("max fre: %lld\n", m_head.m_next->fre);
        m_map.clear();
        m_fasthash->clear();
        ListNode* node = m_head.m_next;
        ListNode* next;
        while (node != &m_tail) {
            next = node->m_next;
            delete node;
            node = next;
        }

        node = m_fast_head.m_next;
        while (node != &m_fast_tail) {
            // Ziyue: presumably unreachable
            next = node->m_next;
            delete node;
            node = next;
        }
        m_fast_head.m_next = &m_fast_tail;
        m_fast_tail.m_prev = &m_fast_head;

        m_head.m_next = &m_tail;
        m_tail.m_prev = &m_head;

        s = 0;
    }

    virtual bool construct_ratio(double FC_ratio) override {
        assert(FC_ratio < 0.999);
        assert(m_fast_head.m_next == &m_fast_tail);
        assert(m_fast_tail.m_prev == &m_fast_head);
        
        std::unique_lock<ListMutex> lockA(m_listMutex);
        FH_construct = true;
        //m_fast_head.m_next = m_head.m_next;
        lockA.unlock();

        size_t FC_size = FC_ratio * m_maxSize;
        //size_t DC_size = m_maxSize - FC_size;
        //printf("FC_size: %lu, DC_size: %lu\n", FC_size, DC_size);

        size_t count = 0;
        HashMapConstAccessor temp_hashAccessor;
        ListNode* out_node = m_head.m_next;
        freListNode* delete_temp;
        while(count < FC_size){
            freListNode* in_node = out_node->list.m_next;
            while(in_node != &out_node->list){
                count++;
                if(! m_map.find(temp_hashAccessor, in_node->m_key)){
                    delete_temp = in_node;
                    printf("fail key %lu, count: %lu\n", delete_temp->m_key, count);
                    // printf("insert count: %lu v.s. eviction count: %lu\n", count, eviction_count);
                    in_node = in_node->m_next;
                    if(delete_temp->isInList())
                        delink(delete_temp);
                    delete delete_temp;
                    //fail_count++;
                    continue;
                }
                m_fasthash->insert(in_node->m_key, temp_hashAccessor->second.val);
                in_node = in_node->m_next;
            }
            out_node = out_node->m_next;
            assert(out_node != &m_tail);
        }
        printf("Insert count: %lu (Aim FC_size: %lu), DC_size: %lu\n", count, FC_size, m_maxSize-count);

        std::unique_lock<ListMutex> lockB(m_listMutex);
        m_fast_head.m_next = m_head.m_next;
        m_head.m_next->m_prev = &m_fast_head;
        m_fast_tail.m_prev = out_node->m_prev;
        out_node->m_prev->m_next = &m_fast_tail;
        //m_tail.m_prev->m_next = &m_fast_tail;
        m_head.m_next = out_node;
        out_node->m_prev = &m_head;
        FH_ready = true;
        FH_construct = false;
        lockB.unlock();
        return true;
    }

    virtual bool construct_tier() override {
        std::unique_lock<ListMutex> lock(m_listMutex);
        FH_construct = true;
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
        ListNode* out_node = m_fast_head.m_next;
        HashMapConstAccessor temp_hashAccessor;
        
        size_t count = 0;
        while(out_node != &m_fast_tail){
            //printf("out_node fre: %d\n", out_node->fre);
            // fflush(stdout);
            freListNode* in_node = out_node->list.m_next;
            while(in_node != &out_node->list){
                if(! m_map.find(temp_hashAccessor, in_node->m_key)){
                    // printf("fail key %d\n", in_node->m_key);
                    // fflush(stdout);
                    auto delete_temp = in_node;
                    in_node = in_node->m_next;
                    delink(delete_temp);
                    delete(delete_temp);
                } else {
                    // printf("success key %d (count: %ld)\n", in_node->m_key, count);
                    // fflush(stdout);
                    m_fasthash->insert(in_node->m_key, temp_hashAccessor->second.val);
                    count++;
                    in_node = in_node->m_next;
                }
            }
            out_node = out_node->m_next;
        }

        printf("fast hash insert num: %ld, m_size: %ld (FC_ratio: %.2lf)\n", count, s.load(), count*1.0/s.load());
        FH_ready = true;
        tier_ready = true;
        FH_construct = false;
        return true;
    }

    virtual void deconstruct() override {
        // assert(fast_hash_ready == false && tier_ready == false);
        assert(!((m_fast_head.m_next == &m_fast_tail) ^ (m_fast_tail.m_prev == &m_fast_head)));
        if(m_fast_head.m_next == &m_fast_tail){
            printf("no need to deconstruct!\n");
            fflush(stdout);
            return;
        }

        // stop freq++ (stop promotion)
        // from end to start, give max_freq+1, ..., max_freq+n to FC list
        std::unique_lock<ListMutex> lockA(m_listMutex);
        FH_construct = true;
        lockA.unlock();

        long long int max_freq = m_head.m_next->fre; // max
        printf("DC max freq: %lld\n", max_freq);

        ListNode* moribund = m_fast_tail.m_prev;

        int count = 0;
        while(moribund != &m_fast_head){
            count++;
            assert(moribund != nullptr);
            moribund->fre = max_freq + count;
            moribund = moribund->m_prev;
        }
        printf("FC has %d list node, and well prepared\n", count);

        std::unique_lock<ListMutex> lockB(m_listMutex);
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
        if(FH_ready)
            FH_ready = false;
        
        FH_construct = false;
        lockB.unlock();

        m_fasthash->clear();

        return;
    }

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
    
    inline void pushForward(ListNode* node, ListNode* new_node){
        assert(node != &m_head);
        assert(new_node->fre > node->fre);
        new_node->m_prev = node->m_prev;
        new_node->m_next = node;
        node->m_prev->m_next = new_node;
        node->m_prev = new_node;
    }

    virtual bool find(TValue& ac, const TKey& key) override {
        bool stat_yes = LFU_FHCache::sample_generator();
        assert(!(tier_ready || FH_ready) || !curve_flag.load());
        bool status_first_ready = FH_ready;

        if(status_first_ready) {
        STATUS_BACK:
            if(m_fasthash->find(key, ac)){
                if(stat_yes)
                    LFU_FHCache::fast_find_hit++;
                return true;
            } else {
                if(tier_ready){
                    if(stat_yes)
                        LFU_FHCache::tbb_find_miss++;
                    return false;
                }
            }
        }

        //HashMapConstAccessor hashAccessor;
        HashMapAccessor hashAccessor; // TODO: replace to const and use replace
        if(!m_map.find(hashAccessor, key)) {
            if(stat_yes)
                LFU_FHCache::tbb_find_miss++;
            return false;
        }
        ac = hashAccessor->second.val;

        if(!FH_construct){
            freListNode* frenode = hashAccessor->second.freNode;
            ListNode* listnode = hashAccessor->second.listNode;
            if(listnode == nullptr){ // TODO
                printf("key: %ld\n", key);
                assert(false);
            }
            auto fre = listnode->fre;

            if(curve_flag.load()){
            CURVE_HANDLE:
                assert(m_threshold != 0);
                if(m_threshold >= fre){
                    if(stat_yes)
                        LFU_FHCache::end_to_end_find_succ++;

                } else if(stat_yes) {
                    LFU_FHCache::fast_find_hit++;
                }
                return true;
            }
            std::unique_lock<ListMutex> lock(m_listMutex);
            if(curve_flag.load())
                goto CURVE_HANDLE;
            
            if(!status_first_ready && FH_ready){
                goto STATUS_BACK;
            }
            
            if(!FH_construct && frenode->isInList()){
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
        }

        if(stat_yes)
            LFU_FHCache::end_to_end_find_succ++;
        return true;
    }

    virtual bool get_curve(bool& should_stop) override{
        assert(!tier_no_insert.load());
        m_threshold = m_head.m_next->fre; // max
        printf("Profiling threshold (max freq): %lld\n", m_threshold);

        // TODO @ Ziyue: before profiling, check tail freq=1 has how many objects

        std::unique_lock<ListMutex> lockA(m_listMutex);
        curve_flag = true;
        LFU_FHCache::reset_cursor();
        LFU_FHCache::sample_flag = false;
        lockA.unlock();

        assert(LFU_FHCache::curve_container.size() == 0);
        auto start_time = SSDLOGGING_TIME_NOW;
        auto last_time = start_time;

        size_t pass_counter = 0;
        size_t FC_size = 0;
        ListNode* out_node = m_head.m_next;
        for(int i = 1; i < 45 && !should_stop; i++){
            while(FC_size <= m_maxSize * i * 1.0/100 * 2 && !should_stop){
                size_t last_count = FC_size;

                auto in_node = out_node->list.m_next;
                while(in_node != &out_node->list){
                    FC_size++;
                    in_node = in_node->m_next;
                }

                printf("Freq: %lld from %lu to %lu\n", m_threshold, last_count, FC_size);

                out_node = out_node->m_next;
                //assert(out_node != &m_tail);
                if(out_node == &m_tail){
                    break;
                }
                m_threshold = out_node->fre;

                double temp_hit = 0, temp_miss = 1;
                LFU_FHCache::get_step(temp_hit, temp_miss);
                //if(temp_hit + temp_miss > 0.99 && FC_size > m_maxSize * 0.2){
                if(temp_hit + temp_miss > 0.99){
                    break;
                }
            }
            if(FC_size > m_maxSize * (i+1) * 1.0/100 * 2)
                continue;
            printf("\ncurve pass %lu\n", pass_counter++);
            double FC_ratio = FC_size * 1.0 / m_maxSize;
            printf("FC_size: %lu (FC_ratio: %.3lf)\n", FC_size, FC_ratio);
            double FC_hit = 0, miss = 1;

            LFU_FHCache::print_step(FC_hit, miss);

            auto curr_time = SSDLOGGING_TIME_NOW;
            printf("duration: %.3lf ms\n", SSDLOGGING_TIME_DURATION(last_time, curr_time)/1000);
            last_time = curr_time;

            //if(FC_hit + miss > 0.99 && FC_size > m_maxSize * 0.2){
            if(FC_hit + miss > 0.99){
                LFU_FHCache::curve_container.insert(LFU_FHCache::curve_container.end(), curve_data_node(1.0, FC_hit, miss));
                break;
            } else
                LFU_FHCache::curve_container.insert(LFU_FHCache::curve_container.end(), curve_data_node(FC_ratio, FC_hit, miss));
        }
        LFU_FHCache::sample_flag = true;
        
        printf("\ncurve container size: %lu\n", LFU_FHCache::curve_container.size());
        printf("Total duration: %.3lf ms\n", SSDLOGGING_TIME_DURATION(start_time, SSDLOGGING_TIME_NOW)/1000);
        std::unique_lock<ListMutex> lockB(m_listMutex);
        curve_flag = false;
        lockB.unlock();

        m_threshold = 0; // reset

        return false;
        
    }
    
    virtual bool insert(const TKey& key, const TValue& value) override {
        bool stat_yes = LFU_FHCache::sample_generator();
        if(stat_yes){
            LFU_FHCache::insert_count++;
        }
        
        if(tier_no_insert.load() || curve_flag.load())
            return false;

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
        std::unique_lock<ListMutex> lock(m_listMutex);

        if(tier_no_insert.load() || curve_flag.load()){
            lock.unlock();
            m_map.erase(hashAccessor);
            delete(node);
            return false;
        }

        ListNode* lfreNode = m_tail.m_prev;
        //printf("insert key: %ld, test listnode fre: %ld\n", key, lfreNode->fre);
        if(lfreNode->fre == 1){
            lfreNode->pushFront(node);
        } else {
            lfreNode = new ListNode(1);
            lfreNode->pushFront(node);
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
        assert(size <= m_maxSize);
        if(size < m_maxSize){
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
                printf("Insert fail No.%d\n", fail_num++);
                return false;
            }
            m_map.erase(hashAccessor);
            delete temp_node;
        }
        return true;
    }
};

/**
 * Your LFU_FHCache object will be instantiated and called as such:
 * LFU_FHCache* obj = new LFU_FHCache(capacity);
 * int param_1 = obj->find(key);
 * obj->insert(key,value);
 */
} // namespace Cache
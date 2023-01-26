#include <unordered_map>
#include "tbb/concurrent_hash_map.h"
#include <iostream>
#include <shared_mutex>
#include <sys/time.h>
#include <unordered_map>
#include <vector>
#include <queue>
#include <random>

#include "baseCache.h"

#define LRU_BITS 24
#define LRU_CLOCK_MAX ((1<<LRU_BITS)-1) /* Max value of obj->lru */
#define LRU_CLOCK_RESOLUTION 100 /* LRU clock resolution in 100us */

#define EVICTION_POOL_SIZE 16
#define EVICTION_SAMPLE 5

namespace Cache {

//unsigned long long getLRUClock(void);

template <class TKey>
class RandomizedSet {
public:
    RandomizedSet() {
        ;
    }
    ~RandomizedSet() {
        PositionMap.clear();
        vec.clear();
    }
    bool insert(TKey key) {
        if(PositionMap.find(key) != PositionMap.end()) {
            return false;
        }
        PositionMap[key] = vec.size();
        vec.push_back(key);
        return true;
    }

    bool remove(TKey key) {
        if(PositionMap.find(key) == PositionMap.end()) {
            return false;
        }
        int pos = PositionMap[key];
        PositionMap[vec.back()] = pos;
        PositionMap.erase(key);
        std::swap(vec[pos], vec[vec.size() - 1]);
        vec.pop_back();
        return true;
    }

    TKey getRandom() {
        // std::random_device rd;
        // std::mt19937 gen(rd());
        // std::uniform_int_distribution<TKey> rng_coin(0, vec.size() - 1);
        // return vec[rng_coin(gen)];
        size_t index = (double)(ssdlogging::random::Random::GetTLSInstance()->Next()) / (double)(RAND_MAX) * vec.size();
        return vec[index];
    }

private:
    // Hash map to record where TKey located in vector
    std::unordered_map<TKey, int> PositionMap;
    std::vector<TKey> vec;
};

template <class TKey, class TValue, class THash = tbb::tbb_hash_compare<TKey>>
class RedisLRUCache : public BaseCacheAPI<TKey, TValue, THash>{

    using BaseCache = Cache::CacheAPI<TKey, TValue, THash>;
    struct evictionPoolEntry {
        unsigned long long idle = 0;    /* Object idle time (inverse frequency for LFU) */
        TKey key = 0;                    /* Key name. */
    };
    struct lru_object {
        lru_object(TKey key) : lru(ustime()), refcount(0), m_key(key) {}
        unsigned lru;
        int refcount;
        TKey m_key;
    };

    struct compareFunction {
        bool operator() (evictionPoolEntry entry_1, evictionPoolEntry entry_2) {
            return entry_1.idle < entry_2.idle;
        }
    };

    struct HashMapValue {
        HashMapValue() : m_lru_object(nullptr) {}

        HashMapValue(const TValue& value, lru_object* node)
            : m_value(value), m_lru_object(node) {}

        TValue m_value;
        lru_object* m_lru_object;
    };

    typedef tbb::concurrent_hash_map<TKey, HashMapValue> HashMap;
    typedef typename HashMap::const_accessor HashMapConstAccessor;
    typedef typename HashMap::accessor HashMapAccessor;
    typedef typename HashMap::value_type HashMapValuePair;
    // typedef std::pair<const TKey, TValue> SnapshotValue;

private:
    size_t n; // max size
    std::atomic<size_t> s;
    evictionPoolEntry* EvictionPool[EVICTION_POOL_SIZE];
    HashMap m_map;

    typedef std::mutex ListMutex;
    ListMutex redis_lock;
    RandomizedSet<TKey> rs;

private:
    unsigned long long estimateObjectIdleTime(lru_object *o) {
        unsigned long long lruclock = ustime();
        if (lruclock >= o->lru) {
            return (lruclock - o->lru) / LRU_CLOCK_RESOLUTION;
        } else {
            return (lruclock + (LRU_CLOCK_MAX - o->lru )) / LRU_CLOCK_RESOLUTION;
        }
    }
    void evictionPoolPopulate() {
        int i, j, count = 0;
        TKey samples[EVICTION_SAMPLE];
        for(int m = 0; m < EVICTION_SAMPLE; m++) {
            samples[m] = 0;
        }
        i = 0;
        std::unordered_map<TKey, TKey> umap;
        HashMapConstAccessor const_accessor;
        while((i < EVICTION_SAMPLE * 2) && (count < EVICTION_SAMPLE)) {
            TKey sampled_key = rs.getRandom();
            if(umap.insert(std::make_pair(sampled_key, sampled_key)).second){
                samples[count] = sampled_key;
                count ++;
            }
            // if(umap.find(sampled_key) == umap.end() ) {
            //     // const_accessor.release();
            //     umap.emplace(sampled_key, sampled_key);
            //     samples[count] = sampled_key;
            //     // std::cout << "sample key " << sampled_key << std::endl;
            //     count ++;
            // }
            i ++;
        }
        //auto start_memmove = SSDLOGGING_TIME_NOW;
        for (i = 0; i < count; i ++) {
            unsigned long long idle = 0;
            if(!m_map.find(const_accessor, samples[i])) {
                const_accessor.release();
                continue;
            }
            
            idle = estimateObjectIdleTime(const_accessor->second.m_lru_object);
            const_accessor.release();
            if(idle == 0) continue;
            j = 0;
            std::unique_lock<ListMutex> lock(redis_lock);
            // if(lock)
            {
                while(j < EVICTION_POOL_SIZE && (*(EvictionPool[j])).key && (*(EvictionPool[j])).idle < idle ) j ++;
                if(j == 0 && (*(EvictionPool[0])).key != 0 && (*(EvictionPool[EVICTION_POOL_SIZE - 1])).key != 0) {
                    // idle time less than worst one, should not put it in pool
                    lock.unlock();
                    continue;
                } else if(j < EVICTION_POOL_SIZE && (*(EvictionPool[j])).key == 0) {
                    // find an empty bucket, we can place it in pool
                } else {
                    // find a middle point, we can insert item here
                    if((*(EvictionPool[EVICTION_POOL_SIZE - 1])).key == 0) {
                        // still has an space right, we can move item right
                        memmove(EvictionPool + j + 1, EvictionPool + j, sizeof(EvictionPool[0]) * (EVICTION_POOL_SIZE - j - 1));
                        // for(int pos = EVICTION_POOL_SIZE - 1; pos > j; pos --) {
                        //     EvictionPool[pos] = EvictionPool[pos - 1];
                        // }
                    } else {
                        // no empty space right, and we should move item left
                        j--;
                        memmove(EvictionPool, EvictionPool + 1, sizeof(EvictionPool[0]) * j);
                        // for(int pos = 0; pos < j; pos ++) {
                        //     EvictionPool[pos] = EvictionPool[pos + 1];
                        // }
                    }
                }
                // if(j == 0 && EvictionPool[EVICTION_POOL_SIZE - 1].key != 0 ) {
                //     continue;
                // } else if (j < EVICTION_POOL_SIZE && EvictionPool[j].key == 0) {

                // } else {
                //     if (EvictionPool[EVICTION_POOL_SIZE - 1].key == 0) {
                //         memmove(EvictionPool + j + 1, EvictionPool + j, sizeof(EvictionPool[0]) * (EVICTION_POOL_SIZE - j - 1));
                //     } else {
                //         j --;
                //         memmove(EvictionPool, EvictionPool + 1, sizeof(EvictionPool[0]) * j);
                //     }
                // }
                (*(EvictionPool[j])).idle = idle;
                (*(EvictionPool[j])).key = samples[i];
                lock.unlock();
            }
        }
        // auto memmove_duration = SSDLOGGING_TIME_DURATION(start_memmove, SSDLOGGING_TIME_NOW);
        // memmove_latency.insert(memmove_duration);
    }

    bool performEviction() {
        TKey evict_key = 0;
        unsigned long long idle = 0;
        HashMapConstAccessor const_accessor;
        HashMapAccessor hashAccessor;
        
        sample:
        for(int i = 0; i < EVICTION_SAMPLE; i++) {
            TKey sampled_key = rs.getRandom();
            if(!m_map.find(const_accessor, sampled_key)) {
                const_accessor.release();
                continue;
            }
            unsigned long long sampled_idle = estimateObjectIdleTime(const_accessor->second.m_lru_object);
            const_accessor.release();
            if( sampled_idle > idle) {
                evict_key = sampled_key;
                idle = sampled_idle;
            }
        }
        
        if(!m_map.find(hashAccessor, evict_key)) {
            // printf("unreachable\n");
            //hashAccessor.release();
            goto sample;
        }
        m_map.erase(hashAccessor);
        //hashAccessor.release();
        
        std::unique_lock<ListMutex> lock(redis_lock);
        rs.remove(evict_key);
        lock.unlock();
        return true;
    }

    bool performEvictions() {
        sample:
        evictionPoolPopulate();
        HashMapAccessor hashAccessor;
        std::unique_lock<ListMutex> lock(redis_lock);
        for (int k = EVICTION_POOL_SIZE - 1; k >= 0; k --) {
            if ((*(EvictionPool[k])).key == 0) {
                continue;
            }
            TKey find_key = (*(EvictionPool[k])).key;
            if(!m_map.find(hashAccessor, find_key)) {
                hashAccessor.release();
                (*(EvictionPool[k])).key = 0;
                (*(EvictionPool[k])).idle = 0;
                continue;
            }
            // debug
            // if(RedisLRUCache::sample_generator()){
            //     auto idle = estimateObjectIdleTime(hashAccessor->second.m_lru_object);
            //     printf("evict object has idle time: %llu\n", idle);
            // }

            m_map.erase(hashAccessor);
            //hashAccessor.release();
            rs.remove((*(EvictionPool[k])).key);
            (*(EvictionPool[k])).key = 0;
            (*(EvictionPool[k])).idle = 0;
            //lock.unlock();
            return true;
        }
        lock.unlock();
        goto sample;
    }

public:
    RedisLRUCache(size_t capacity) {
        n = capacity;
        s = 0;
        // EvictionPool = (evictionPoolEntry*)malloc(EVICTION_POOL_SIZE * sizeof(evictionPoolEntry));
        for(int i = 0; i < EVICTION_POOL_SIZE; i++) {
            EvictionPool[i] = (evictionPoolEntry*)malloc(sizeof(evictionPoolEntry));
            (*(EvictionPool[i])).key = 0;
            (*(EvictionPool[i])).idle = 0;
        }
        //printf("Eviction pool size: %d, sample size: %d\n", EVICTION_POOL_SIZE, EVICTION_SAMPLE);
        //RedisLRUCache::reset_stat();
    }

    virtual ~RedisLRUCache() {
        clear();
    }
    virtual void clear() override {
        for(auto x = m_map.begin(); x != m_map.end(); ++x){
            delete x->second.m_lru_object;
        }
        m_map.clear();
        //size_t total_access = 0;
        //printf("cache miss ratio: %lf\n", this->_get_miss_ratio(total_access));
        s = 0;
    }
    virtual size_t size() const override { return s.load(); }

    virtual bool find(TValue& ac, const TKey& key) override {
        HashMapConstAccessor hashAccessor;
        if(!m_map.find(hashAccessor, key)) {
            if(RedisLRUCache::sample_generator())
                RedisLRUCache::miss_num++;
            return false;
        }
        hashAccessor->second.m_lru_object->lru = ustime();
        ac = hashAccessor->second.m_value;
        if(RedisLRUCache::sample_generator())
            RedisLRUCache::hit_num++;
        return true;
    }

    virtual bool insert(const TKey& key, const TValue& value) override {
        lru_object* node = new lru_object(key);
        HashMapAccessor hashAccessor;
        HashMapValuePair hashMapValue(key, HashMapValue(value, node));
        if(!m_map.insert(hashAccessor, hashMapValue)) {
            delete node;
            return false;
        }
        hashAccessor.release();
        std::unique_lock<ListMutex> lock(redis_lock);
        rs.insert(key);
        lock.unlock();
        if(s.load() < n) {
            s++;
        } else {
            performEvictions();
        }
        return true;
        // if(lock) 
        // {
        //     size_t size = s.load();
        //     if(size < n) {
        //         s.store(size + 1);
        //         lock.unlock();
        //         return true;
        //     }
        //     else{ // evict
        //         lock.unlock();
        //         performEviction();
        //         return true;
        //     }
        // }
        
    }
};

/*static long long mstime(void) {
    struct timeval tv;
    long long mst;

    gettimeofday(&tv, NULL);
    mst = ((long long)tv.tv_sec)*1000;
    mst += tv.tv_usec/1000;
    return mst;
}*/

// static long long ustime(void) {
//     struct timeval tv;
//     long long ust;

//     gettimeofday(&tv, NULL);
//     ust = ((long)tv.tv_sec)*1000000;
//     ust += tv.tv_usec;
//     return ust;
// }

// unsigned long long getLRUClock(void) {
//     return ustime();
// }

}
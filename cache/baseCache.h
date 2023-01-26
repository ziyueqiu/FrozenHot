#ifndef incl_BASE_CACHE_H
#define incl_BASE_CACHE_H

#include "../ssdlogging/util.h"
#include "cache.h"

namespace Cache
{
    template <class TKey, class TValue, class THash = tbb::tbb_hash_compare<TKey>>
    class BaseCacheAPI : public CacheAPI<TKey, TValue, THash> {
    public:
        virtual ~BaseCacheAPI() {}
        
        virtual void thread_init(int tid) override {}

        virtual void clear() override {}

        virtual void snapshotKeys(std::vector<TKey>& keys) override {}

        virtual size_t size() const override = 0;

        virtual void evict_key() override {}

        virtual void ResetStat() override {}

        virtual void PrintStat(size_t& hit, size_t& miss) override {
            hit = hit_num.load();
            miss = miss_num.load();
            if(hit + miss != 0)
                printf("hit rate: %.4lf, hit num: %lu, miss num: %lu\n", 
                    hit*1.0/(hit+miss), hit, miss);
            hit_num = 0;
            miss_num = 0;
        }

        virtual void GetStat(size_t& hit, size_t& miss) override {
            hit = hit_num.load();
            miss = miss_num.load();
            hit_num = 0;
            miss_num = 0;
        }

        virtual bool find(TValue& ac, const TKey& key) override = 0;

        virtual bool insert(const TKey& key, const TValue& value) override = 0;

        virtual void delete_key(const TKey& key) override {}

        virtual double _get_miss_ratio(size_t& total_access) override {
            double temp = 1;
            total_access = hit_num.load()+miss_num.load();
            if(total_access != 0)
                temp = miss_num.load()*1.0/total_access;
            hit_num = 0;
            miss_num = 0;
            return temp;
        }

        // static long long ustime(void) {
        //     struct timeval tv;
        //     long long ust;

        //     gettimeofday(&tv, NULL);
        //     ust = ((long)tv.tv_sec)*1000000;
        //     ust += tv.tv_usec;
        //     return ust;
        // }

        /*static long long mstime(void) {
            struct timeval tv;
            long long mst;

            gettimeofday(&tv, NULL);
            mst = ((long long)tv.tv_sec)*1000;
            mst += tv.tv_usec/1000;
            return mst;
        }*/

        virtual void reset_stat() override {
            hit_num = 0;
            miss_num = 0;
        }

        void print_reset_stat() {
            auto hit = hit_num.load();
            auto miss = miss_num.load();
            if(hit + miss != 0)
                printf("hit rate: %.4lf, hit num: %lu, miss num: %lu\n", 
                    hit*1.0/(hit+miss), hit, miss);
            hit_num = 0;
            miss_num = 0;
        }

        inline bool sample_generator() {
            return ((double)(ssdlogging::random::Random::GetTLSInstance()->Next()) / (double)(RAND_MAX)) < sample_percentage;
        }
        std::atomic<size_t> hit_num{0};
        std::atomic<size_t> miss_num{0};
        const double sample_percentage = 1.0/100;
        
    };
} // namespace Cache

#endif //incl_BASE_CACHE_H
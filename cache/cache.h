#ifndef incl_CACHE_H
#define incl_CACHE_H

#include <tbb/concurrent_hash_map.h>
namespace Cache
{
    struct curve_data_node {
        double size_, FC_hit_, miss_;
        curve_data_node(double size = 0, double FC_hit = 0, double miss = 1):
        size_(size), FC_hit_(FC_hit), miss_(miss) {}
    };

    static long long ustime(void) {
            struct timeval tv;
            long long ust;

            gettimeofday(&tv, NULL);
            ust = ((long)tv.tv_sec)*1000000;
            ust += tv.tv_usec;
            return ust;
    }

    // static long long mstime(void) {
    //     struct timeval tv;
    //     long long mst;

    //     gettimeofday(&tv, NULL);
    //     mst = ((long long)tv.tv_sec)*1000;
    //     mst += tv.tv_usec/1000;
    //     return mst;
    // }

    template <class TKey, class TValue, class THash = tbb::tbb_hash_compare<TKey>>
    class CacheAPI {
    public:
        virtual ~CacheAPI() {}
        
        virtual void thread_init(int tid) {}

        virtual void clear() {}

        virtual void snapshotKeys(std::vector<TKey>& keys) {}

        virtual size_t size() const = 0;

        virtual void evict_key() {}

        virtual void ResetStat() {}

        virtual void PrintStat() {}

        virtual void PrintStat(size_t& hit, size_t& miss) {}

        virtual void GetStat(size_t& hit, size_t& miss) {}

        virtual void GetStat(size_t& FH_hit, size_t& O_hit, size_t& miss) {}

        virtual bool find(TValue& ac, const TKey& key) = 0;

        virtual bool insert(const TKey& key, const TValue& value) = 0;

        virtual void delete_key(const TKey& key) {}

        virtual double _get_miss_ratio(size_t& total_access) { return 1; }

        virtual void reset_stat() {}
        virtual void reset_cursor() {}
        virtual void print_step() {}
        virtual void get_step(double& FC_hit, double& global_miss) {}
        virtual void print_step(double& FC_hit, double& global_miss) {}
        virtual double print_reset_fast_hash() { return 1;}
        virtual double print_fast_hash() { return 1; }

        virtual bool find_marker(TValue& ac, const TKey& key) { return false; }

        //virtual bool construct_from(const TKey& key) { return false; }

        /* this is a function to construct the FC cache from a given ratio */
        virtual bool construct_ratio(double FC_ratio) { return false; }

        /* this is a function to construct the 100% FC */
        virtual bool construct_tier() { return false; }

        virtual void deconstruct() {}
        virtual bool get_curve(bool& should_stop) { return false;}

        virtual bool is_full() { return false;}

        bool sample_flag = true;
        std::vector<curve_data_node> curve_container;

        std::atomic<int> promotion_counter{0};
    };
} // namespace Cache

#endif //incl_CACHE_H
#pragma once
#include "../cache/hhvm-scalable-cache.h"
#include "trace_loader.h"
#include <functional>
#include "../ssdlogging/util.h"
#include "../ssdlogging/properties.h"
#include <thread>
#include <map>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// actually useless
#define FH_ENABLE

// depends on the cpu type
// the goal is to make use of all cores in same socket
// #define XEONR8360

#define Param_Granularity 1
#define Large_Param_Granularity 1000

class Client{
    public:
    Client(utils::Properties props){
        seg = atoi((props.GetProperty("seg num")).c_str());
        cache_size = atof((props.GetProperty("cache size")).c_str());
        request_num = atoll((props.GetProperty("request num")).c_str());
        work_thread = atoi((props.GetProperty("threads num")).c_str());
        param = props.GetProperty("workload file or zipf const");
        work_type = props.GetProperty("workload type");
        cache_type = props.GetProperty("cache type");
        disk_lat = atoi((props.GetProperty("disk lat")).c_str());
        rebuild_freq = atoi((props.GetProperty("rebuild freq")).c_str());
        // TODO @ Ziyue: unaware of thread num in real case
        Large_Granularity = work_thread * Large_Param_Granularity;
        Granularity = work_thread * Param_Granularity;

        twitter_start = props.GetProperty("workload start", "");
        twitter_end = props.GetProperty("workload end", "");
        if(! work_type.compare("Zipf")){
            loader_1 = new TraceLoader(request_num, atof(param.c_str()));
        } else if(! work_type.compare("MSR")){
            loader_1 = new TraceLoader(request_num, param, work_type);
        } else if(! work_type.compare("Twitter")) {
            // binary format
            loader_1 = new TraceLoader(request_num, param, work_type, twitter_start, twitter_end, true);
            // original format
            // loader_1 = new TraceLoader(request_num, param, work_type, twitter_start, twitter_end);
        } else {
            printf("cannot find workload type: %s\n", work_type.c_str());
            assert(false);
        }
        queue_size = loader_1->GetQueueSize();
        int real_cache_size = 0;
        real_cache_size = cache_size;
        if(! cache_type.compare("LRU")) {
            scalable_cache = new Cache(real_cache_size, seg, Cache::LRU);
        } else if(! cache_type.compare("LFU")) {
            scalable_cache = new Cache(real_cache_size, seg, Cache::LFU);
        } else if(! cache_type.compare("LRU_FH")) {
            scalable_cache = new Cache(real_cache_size, seg, Cache::LRU_FH, rebuild_freq);
            is_fh_cache_type = true;
        } else if(! cache_type.compare("LFU_FH")) {
            scalable_cache = new Cache(real_cache_size, seg, Cache::LFU_FH, rebuild_freq);
            is_fh_cache_type = true;
        } else if(! cache_type.compare("FIFO_FH")) {
            scalable_cache = new Cache(real_cache_size, seg, Cache::FIFO_FH, rebuild_freq);
            is_fh_cache_type = true;
        } else if(! cache_type.compare("Redis_LRU")) {
            scalable_cache = new Cache(real_cache_size, seg, Cache::Redis_LRU);
        } else if(! cache_type.compare("FIFO")) {
            scalable_cache = new Cache(real_cache_size, seg, Cache::FIFO);
        } else if(! cache_type.compare("StrictLRU")) {
            scalable_cache = new Cache(real_cache_size, seg, Cache::StrictLRU);
        }
        else {
            printf("cannot find cache type: %s\n", cache_type.c_str());
            assert(false);
        }
        printf("%d items per shard\n", real_cache_size / seg);
    }

    ~Client(){
        if(loader_1 != nullptr)
            delete loader_1;
        if(scalable_cache != nullptr)
            delete scalable_cache;
    }
    public:
    TraceLoader* loader_1;
    
    tbb::tbb_hash_compare<size_t> hashObj;
    int shift = std::numeric_limits<size_t>::digits - 16;

    typedef tstarling::ConcurrentScalableCache<size_t, std::shared_ptr<std::string>> Cache;
    Cache *scalable_cache = nullptr;
    int seg;
    std::string work_file_1;
    double cache_size;
    double zipf_const;
    long long request_num;
    int work_thread;
    std::string param;
    std::string work_type;
    std::string cache_type;
    bool is_fh_cache_type = false;
    int disk_lat;
    int rebuild_freq;
    size_t Large_Granularity;
    size_t Granularity;

    /** start and end file used in Twitter trace*/
    std::string twitter_start;
    std::string twitter_end;

	int base_coreid = 0;
    uint64_t queue_size;
    public:
    inline void SetAffinity(int coreid){
        printf("client coreid: %d\n", coreid);
        fflush(stdout);
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(coreid, &mask);
        int rc = sched_setaffinity(syscall(__NR_gettid), sizeof(mask), &mask);
        assert(rc == 0);
    }

    void FastHashMonitor(int coreid){
        SetAffinity(coreid);
        if(! is_fh_cache_type)
            scalable_cache->BaseMonitor();
        else
            scalable_cache->FastHashMonitor();
    }

    void warmup(){
        std::vector<std::thread> threads;
        std::function<void(uint64_t, int, bool, bool, uint64_t)> fn;
#ifdef FH_ENABLE
        std::vector<std::thread> threads_monitor;
        std::function< void(int)> fn_monitor;
        printf("client2 have monitor!\n");
        
        printf("granularity: %lu and large granularity: %lu\n", Granularity, Large_Granularity);
#endif
        fn = std::bind(&Client::worker_thread, this, 
                            std::placeholders::_1, std::placeholders::_2, 
                            std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
#ifdef FH_ENABLE
        fn_monitor = std::bind(&Client::FastHashMonitor, this, std::placeholders::_1);
#endif
        const uint64_t single_num = request_num / work_thread;
        auto start_work = SSDLOGGING_TIME_NOW;
        #ifdef XEONR8360
            printf("use cpu - xeonr8360\n");
        #else
            printf("use cpu - other\n");
        #endif
        for(int i = 0; i < work_thread; i++){
            uint64_t start_ = queue_size / work_thread * i;
        #ifdef XEONR8360
            threads.emplace_back(fn, single_num, i * 2, true, i==0, start_);
        #else
            if(base_coreid + i > 39)
                threads.emplace_back(fn, single_num, base_coreid + i + 40, true, i==0, start_);
            else
                threads.emplace_back(fn, single_num, base_coreid + i, true, i==0, start_);
        #endif
        }
#ifdef FH_ENABLE
        #ifdef XEONR8360
            threads_monitor.emplace_back(fn_monitor, 1);
        #else
            threads_monitor.emplace_back(fn_monitor, 0);
        #endif
#endif
        for(auto &t : threads){
            t.join();
        }
        auto work_duration = SSDLOGGING_TIME_DURATION(start_work, SSDLOGGING_TIME_NOW);
#ifdef FH_ENABLE
        scalable_cache->monitor_stop();
        for(auto &t: threads_monitor){
            t.join();
        }
#endif
        printf("\nAll threads run %lf s\n", work_duration / 1000 / 1000);
        fflush(stdout);
        scalable_cache->PrintGlobalLat();
    }

    inline void busy_sleep(std::chrono::nanoseconds t) {
        auto end = std::chrono::steady_clock::now() + t;
        while(std::chrono::steady_clock::now() < end);
    }

    void worker_thread(uint64_t num, int coreid, bool is_warmup, bool is_master, uint64_t start_offset){
        SetAffinity(coreid);
        if(!is_warmup && is_master){
            printf("starting requests...\n");
        }
        int offset = coreid > 40 ? coreid - 40: coreid;
        
        scalable_cache->thread_init(coreid);

        std::chrono::_V2::system_clock::time_point start_of_loop;
        for(uint64_t i = 0; i < num; i++){
            auto req = loader_1->GetRequest(offset + i * work_thread);
            auto key = req.Key();
            assert(key != 0);

            // debug
            int succount = 0;

            bool ret_flag = false; // TODO @ Ziyue: how to consider set()/insert()
            TraceLoader::Operation opt = req.Type();
            if((!scalable_cache->stop_sample_stat && i % Granularity == 0) || (scalable_cache->stop_sample_stat && i % Large_Granularity == 0))
                start_of_loop = SSDLOGGING_TIME_NOW;
            if(opt == TraceLoader::Operation::READ){
                std::shared_ptr<std::string> r_value;
                if(scalable_cache->find(r_value, key)){
                    ret_flag = true;
                }
                else{
                    busy_sleep(std::chrono::microseconds(disk_lat));
                    std::shared_ptr<std::string> w_value(new std::string(1, 'a')); // just metadata; no real data
                    scalable_cache->insert(key, w_value);
                }
            }
            else if(opt == TraceLoader::delete_){
                scalable_cache->delete_key(key);
            }
            else {
                std::shared_ptr<std::string> r_value;
                if(scalable_cache->find(r_value, key)){
                    ret_flag = true;
                }
                else{
                    busy_sleep(std::chrono::microseconds(disk_lat));
                    std::shared_ptr<std::string> w_value(new std::string(1, 'a'));
                    scalable_cache->insert(key, w_value);
                }
            }
            if(!scalable_cache->stop_sample_stat && (i % Granularity == 0)) {
                auto duration = SSDLOGGING_TIME_DURATION(start_of_loop, SSDLOGGING_TIME_NOW);
                tstarling::req_latency_[(hashObj.hash(key) >> shift) % seg].insert(duration);
                if(i % Large_Granularity == 0) {
                    if(ret_flag)
                        tstarling::total_hit_latency_.insert(duration);
                    else
                        tstarling::total_other_latency_.insert(duration);
                }
            } else if(scalable_cache->stop_sample_stat && (i % Large_Granularity == 0)) {
                auto duration = SSDLOGGING_TIME_DURATION(start_of_loop, SSDLOGGING_TIME_NOW);
                if(ret_flag){
                    tstarling::total_hit_latency_.insert(duration);
                }
                else{
                    // printf("succ count: %    d, duration: %d\n", succount, duration);
                    tstarling::total_other_latency_.insert(duration);
                }
            }
        }
        if(is_warmup)
            return ;
    }
};

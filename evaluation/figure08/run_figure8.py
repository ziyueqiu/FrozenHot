import os
Twitter_prefix = "TBF" # to be filled
MSR_prefix = "TBF" # to be filled

# "trace name":[{cache size: req num}, [covered subtraces]]
Twitter_list = {
    "cluster17": [{
        1000000:500,  # large size for MS machine
        }, [0, 9]],

    "cluster45": [{
        30000000:150,
        # 30000000:1050, # only for rebuild freq
        }, [0, 0]],

    "cluster18": [{
        2400000: 400,
    }, [0, 12]],

    "cluster24": [{
        2000000: 200,
    }, [0, 3]],

    "cluster44": [{
        40000000: 600,
        # 40000000: 7500, # only for rebuild freq
    }, [0, 5]],

    "cluster52": [{
        88000000: 800,
        # 88000000: 10000, # only for rebuild freq
    }, [0, 6]],

    "cluster29": [{
        900000: 100,
    }, [0, 6]],
}

MSR_list = {
    "prn_0":[3886547, 60],
    "prn_1":[21209710, 60],
    "proj_1":[183106310,
        60,
        #300, # only for rebuild freq
        ],
    "proj_2":[107516091,
        60,
        #180, # only for rebuild freq
        ],
    "proj_4":[32453536, 60],
    "prxy_0":[230657,60],
    "prxy_1":[3593602,
        60,
        #120, # only for rebuild freq
        ],
    "src1_0":[31787377, 60],
    "src1_1":[31545608, 60],
    "usr_1":[172674631,
        60,
        #300, # only for rebuild freq
        ],
    "usr_2":[99463868,
        60,
        #180, # only for rebuild freq
        ],
    "web_2":[17636571, 60],
}

workload_types = [
    "Twitter",
    # "Zipf",
    "MSR"
]
seg = [
    16
]
thread_num = [
    1,
    20,
    40,
    60,
    72,
]

# this can change in different machine
Zipf_size_ratio = {
    # TODO: note that 100% hit is not supported in FH
    0.5:120, # 97.5%
    0.25:60, # 90%
    0.004:40, # 50%
}
MSR_size_ratio = {
    0.1,
}
cache_types = [
    "LRU_FH",
    "LRU",

    "FIFO_FH",
    "FIFO",

    "LFU_FH",
    "LFU",

    # "Redis_LRU",
    # "StrictLRU",
]
zipf = [
    0.99,
]

disk_lat = [
    5,
    # 15, 25, 35, 45, 55, 100, 500, # only for sensitivity analysis
]

FH_rebuild_freq = [
    # 100, # only for lifetime factor
    20, # by default (most experiments)
    # 10, # only for lifetime factor
]

for thread in thread_num:
    for shard in seg:
        for lat in disk_lat:
            for cache_type in cache_types:
                # log format
                path_1 = "log_" + str(shard) + "shard_"+ str(lat) + "u_" + str(thread) + "thd_"

                if cache_type in ["LRU_FH", "FIFO_FH", "LFU_FH"]:
                    freq_list = FH_rebuild_freq
                else:
                    freq_list = [0]

                for freq in freq_list:
                    for workload_type in workload_types:
                        # <threads num> <cache size> <request num> <seg num> <workload type>
                        # <workload file(s) (if not zipf)> <Zipf const (if zipf)> <cache type>
                        # <disk lat>
                        path_2 = path_1 + workload_type + "_"

                        if workload_type == "Zipf":
                            if thread <= 2:
                                thread_ = thread * 2
                            else:
                                thread_ = thread
                            for ratio in Zipf_size_ratio:
                                cache_size = int(1000000 * ratio)
                                request_num = Zipf_size_ratio[ratio] * 1000000 * thread_ # rewrite
                                for zipf_const in zipf:
                                    output_file = path_2 + str(zipf_const) + "_" + str(ratio) + "size_" + cache_type + "_rebuild" + str(freq) + ".txt"
                                    command = "./build/test_trace " + str(thread) + " " + str(cache_size) \
                                            + " " + str(request_num) + " " + str(shard) + " " \
                                            + workload_type + " " + str(zipf_const)+ " " + cache_type \
                                            + " " + str(lat) + " " + str(freq) + " > " + output_file
                                    print(command)
                                    os.system(command)
                        elif workload_type == "MSR":
                            if thread <= 2:
                                thread_ = thread * 4 # need warmup
                            else:
                                thread_ = thread
                            for ratio in MSR_size_ratio:
                                for trace in MSR_list:
                                    if trace == "prxy_1" and thread == 1: # need warmup
                                        thread_ = thread * 12
                                    elif trace == "prxy_1" and thread == 20: # need warmup
                                        thread_ = thread * 4
                                    request_num = MSR_list[trace][1] * 1000000 * thread_ # rewrite
                                    output_file = path_2 + trace + "_" + str(ratio) + "size_" + cache_type + "_rebuild" + str(freq) + ".txt"
                                    cache_size = int(MSR_list[trace][0] * ratio)
                                    trace_path = MSR_prefix + trace + ".csv"
                                    os.system("sync; echo 1 > /proc/sys/vm/drop_caches")
                                    # numactl to avoid numa impacts
                                    command = "numactl --membind=0 ./build/test_trace " + str(thread) + " " + str(cache_size) \
                                            + " " + str(request_num) + " " + str(shard) + " " \
                                            + workload_type + " " + trace_path + " " + cache_type \
                                            + " " + str(lat) + " " + str(freq) + " > " + output_file
                                    print(command)
                                    os.system(command)
                        elif workload_type == "Twitter":
                            if thread == 1: # need warmup
                                thread_ = thread * 7.5
                            else:
                                thread_ = thread

                            for trace in Twitter_list:
                                if (trace == "cluster45" or trace == "cluster17") and thread == 1: # write heavy
                                    thread_ = thread * 4
                                elif (trace == "cluster44" or trace == "cluster52") and thread == 1: # need warmup
                                    thread_ = thread * 15
                                
                                for cache_size in Twitter_list[trace][0]:
                                    output_file = path_2 + trace + "_" + str(cache_size) + "_" + cache_type + "_rebuild" + str(freq) + ".txt"
                                    start = Twitter_list[trace][1][0]
                                    end = Twitter_list[trace][1][1]
                                    request_num = Twitter_list[trace][0][cache_size] * 1000000 * thread_ # rewrite

                                    trace_path = Twitter_prefix + trace
                                    os.system("sync; echo 1 > /proc/sys/vm/drop_caches")
                                    command = "numactl --membind=0 ./build/test_trace " + str(thread) + " " + str(cache_size) \
                                            + " " + str(request_num) + " " + str(shard) + " " \
                                            + workload_type + " " + trace_path + " " + cache_type \
                                            + " " + str(lat) + " " + str(freq) + " " + str(start) + " " + str(end) + " > " + output_file
                                    print(command)
                                    os.system(command)
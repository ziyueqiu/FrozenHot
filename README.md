# FrozenHot Cache: Rethinking Cache Management for Modern Software

This repository contains simulation code and instructions to reproduce key results in the FrozenHot paper (to appear in EuroSys'23).

## Repo Introduction

Cache implementations can be found under `cache/`, including `hhvm_lru_cache_FH.h` (LRU-FH), `hhvm_lru_cache.h` (LRU), `lfu_FH.h` (LFU), `lfu.h` (LFU), `redis_lru.h` (Redis-LRU), `FIFO_FH.h` (FIFO), and `FIFO.h` (FIFO).

We use utils in `ssdlogging/` from SPDK, e.g. random generator, current time, distribution stats, etc.

The framework of this benchmark is organized by `traceloader/client.h`, `traceloader/trace_loader.h`, and `test_trace.cpp`.

## Deployment & Usage

```
git clone this_repo
cd this_repo
git submodule update --init --recursive

# compile CLHT
cd CLHT && ./prepare_everything.sh
cd ..

# lib preparation (ubuntu as an example)
sudo apt update
sudo apt install cmake libtbb-dev numactl

# compile project
./prepare.sh

# fill param in run.py and then test by yourself
python run_no_trace.py
```

To satisfy hardware requirements (up to 72 cores in the same socket), enable hyperthreading to use cores from a single socket.

```
# enable hyperthreading
echo on > /sys/devices/system/cpu/smt/control
```

See `#define XEONR8360` in `traceloader/client.h`. Change those codes to use the correct CPU ids. It is not hardware-aware now. The goal is to set client threads on each cpu within single socket.

### Additional Instruction

Run MSR Cambridge traces in the original format, but better run Twitter traces in binary format (to save time and memory overhead). If you are going to run Twitter traces in the original format, you need to change this call `loader_1 = new TraceLoader(request_num, param, work_type, twitter_start, twitter_end, true);` (traceloader/client.h).

Note that we assume the machine has at least 100 GiB memory (see `traceloader/trace_loader.h` code: `requests_ = (request*)malloc(100ull*1024*1024*1024);`) in numa node 0 (see example python script code: `numactl --membind=0 xxx`).

## Key Results

- Figure 8: Throughput improvement for all algorithm workload combinations, with increasing thread counts.
- Figure 9: FC hit share with FH versions of algorithms
- Figure 13: FH sensitivity to FC lifetime factor
- Figure 14: LRU vs. FIFO, before and after enabling FH
- Figure 15: Sensitivity analysis with Zipf 0.99 at 72 threads.

See details in `evaluation` folder.

The experiments might take hours to tens of hours.

## License

This software is licensed under the Apache 2.0 license, see LICENSE for details.
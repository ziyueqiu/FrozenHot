#pragma once
#include <string>
#include <cassert>
#include <unistd.h>
#include <vector>
#include "chrono"
#include <thread>
#include <functional>
#include <fstream>
#include <iostream>
#include "zipfian.h"
#include <random>

#include <sys/sysinfo.h> // for sysinfo()
#define key_size_t uint32_t

unsigned int string_hash(std::string const& s);
std::vector<std::string> extract_suffix_string(std::string const& start, std::string const& end);

class TraceLoader{
  public:
    enum Operation{
        PUT,
        GET,
        DELETE,
        READ,
        WRITE,

        /** following operations are used in memcached trace */

        /** store this data */
        set,
        /** store this data, only if data not already exist; if add fails, promote anyway */
        add, 
        /** store this data, only if data alreay exist */
        replace, 
        /** add this data after last byte in an existing item */
        append,
        /** similar to append, but before existing data */
        prepend,
        /** store this data, only if no one else update it since you read it last */
        cas,

        /** get an item */
        get,
        /** todo */
        gets,
        /** delete an item*/
        delete_,
        /** increase an item */
        incr,
        /** decrease an item */
        decr,

        NoType
    };
    struct request
    {
        private:
        TraceLoader::Operation opt_;
        //int excute_time_;
        key_size_t key_;
        //int value_size_;

        public:
        request():
        opt_(NoType),
        key_(0){}
        
        request(TraceLoader::Operation opt, key_size_t key):
        opt_(opt),
        key_(key){}

        TraceLoader::Operation Type(){ return opt_;}
        size_t Key(){ return key_;}
    };
    TraceLoader(){}
    
    ~TraceLoader(){
        if(requests_ != nullptr)
            free(requests_);
        requests_ = nullptr;
    }

    // Zipf
    TraceLoader(const uint64_t num, double zipf_const){
        printf("using Zipfian as test trace!\n");
        size_t item_num = 1000000;
        size_t workload_size = item_num * 4 / 1024;
        printf("workset size: %lu MB\n", workload_size);
        printf("loading workload (%lu)...\n", num);

        auto zipf_generator = new ScrambledZipfianGenerator(0, item_num, zipf_const);

        auto start = std::chrono::high_resolution_clock::now();
        requests_ = (request*)malloc(item_num * 100 * sizeof(request));
        for(size_t i = 0; i < item_num * 100; i++){
            uint64_t next_ = zipf_generator->Next();
            if (i%16) requests_[i] = request(TraceLoader::READ, next_);
            else requests_[i] = request(TraceLoader::delete_, next_);
        }
        // // pre-fault, to avoid page fault
        // for(size_t i = 0; i < item_num * 100; i++){
        //     auto req = requests_[i];
        // }
        
        queue_size = item_num * 100; // run 100 * working set size
        std::cout << "origin data size :" << queue_size << std::endl;
        auto end = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double>(end - start).count();
        printf("loading workload finished in %.3lf s\n", time);
        fflush(stdout);

        delete zipf_generator;
    }

    // MSR
    TraceLoader(const uint64_t num, std::string work_file, std::string work_type){
        if(! work_type.compare("MSR")) {
            printf("using MSR as test trace!\n");
        }else {
            printf("using unknown work type workload!\n");
            exit(0);
        }
        num_ = num;
        load_index_ = 0;
        printf("loading workload (%lu)...\n", num_);
        auto start = std::chrono::high_resolution_clock::now();
        std::string str;
        std::string op = "";
        std::ifstream infile; 
        infile.open(work_file);
        if( !infile.good()){
            std::cout << "open file fail!" << work_file << std::endl;
            exit(0);
        }

        requests_ = (request*)malloc(100ull*1024*1024*1024); // allocate 100GB (as much as possible)
        if(requests_ == NULL) {
            printf("malloc failed!\n");
            exit(0);
        }
        while(infile.good() && !infile.eof() && load_index_ < num_){
            getline(infile, str);
            if(str == "") continue;
            std::string space_delimiter = ",";
            std::vector<std::string> words{};
            words.clear();
            size_t pos = 0;
            while ((pos = str.find(space_delimiter)) != std::string::npos) {
                words.push_back(str.substr(0, pos));
                str.erase(0, pos + space_delimiter.length());
            }
            words.push_back(str.substr(0, pos));
            int start = strtol(words[4].c_str(), NULL, 10) >> 12;
            int end = (strtol(words[4].c_str(), NULL, 10) + strtol(words[5].c_str(), NULL, 10) - 1) >> 12;
            op = words[3];
            if(op == "Read"){
                for(int index = start; index <= end && load_index_ < num_; index++){
                    requests_[load_index_] = request(TraceLoader::READ, index);
                    load_index_ ++;
                }
            }
            else if(op =="Write"){
                for(int index = start; index <= end && load_index_ < num_; index++){
                    requests_[load_index_] = request(TraceLoader::READ, index); // regarded as read
                    load_index_ ++;
                }
            }
            else{
                std::cout << "error parameter" << op << std::endl;
            }
        }
        infile.close();
        
        queue_size = load_index_;
        std::cout << "origin data size :" << queue_size << std::endl;
        printf("\n");
        auto end = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double>(end - start).count();
        printf("loading workload finished in %.3lf s\n", time);
        fflush(stdout);
    }
    
    // Twritter Binary (our own format)
    TraceLoader(const uint64_t num, std::string work_file, std::string work_type, std::string workload_start, std::string workload_end, bool binary){
        if(binary & !work_type.compare("Twitter"))
            printf("using Twitter binary as test trace!\n");
        else {
            printf("using unknown input as test trace!\n");
            exit(0);
        }

        std::map<int, TraceLoader::Operation> uint8_op_map = {
            {1, TraceLoader::Operation::set},
            {2, TraceLoader::Operation::add},
            {3, TraceLoader::Operation::replace},
            {4, TraceLoader::Operation::append},
            {5, TraceLoader::Operation::prepend},
            {6, TraceLoader::Operation::cas},
            {7, TraceLoader::Operation::get},
            {8, TraceLoader::Operation::gets},
            {9, TraceLoader::Operation::delete_},
            {10, TraceLoader::Operation::incr},
            {11, TraceLoader::Operation::decr}
        };

        num_ = num;
        load_index_ = 0;
        printf("loading workload (%lu)...\n", num_);
        printf("\nper request size: %lu\n", sizeof(request));
        auto start = std::chrono::high_resolution_clock::now();
        std::ifstream infile;
        std::vector<std::string> suffixs;
        if(workload_start == "" || workload_end == "") {
            suffixs.push_back(work_file);
        } else {
            suffixs = extract_suffix_string(workload_start, workload_end);
        }
        
        struct sysinfo sys_info;
        if(sysinfo(&sys_info) != 0) {
            printf("get sysinfo failed!\n");
            exit(0);
        }

        // @Ziyue: transform into numa node freeram
        printf("Total RAM: %llu GB\tFree: %llu GB\n",
            sys_info.totalram * (unsigned long long)sys_info.mem_unit / (1024 * 1024 * 1024),
            sys_info.freeram * (unsigned long long)sys_info.mem_unit / (1024 * 1024 * 1024));
        
        requests_ = (request*)malloc(100ull*1024*1024*1024);
        if(requests_ == NULL) {
            printf("malloc failed!\n");
            exit(0);
        }

        auto infile_read_size = sizeof(int) * 2 + sizeof(uint32_t);
        int file_request_num = 0;
        for(auto suffix : suffixs){
            if (load_index_ >= num_) break;
            if(workload_start == "" || workload_end == ""){
                infile.open(suffix);
            } else {
                infile.open(work_file + "." + suffix + ".bin", std::ifstream::binary);

                infile.seekg(0, infile.end);
                file_request_num = infile.tellg() / infile_read_size;
                printf("file %s length: %d\n", suffix.c_str(), file_request_num);
                infile.seekg(0, infile.beg);

                std::cout << "open file :" + work_file + "." + suffix + ".bin" << std::endl;
            }
            if(!infile.good()) {
                std::cout << "Open input file fail!" << std::endl;
                exit(0);
            }

            // as request size = 8B, we can read 1024 requests each time; to save execution time
            int request_buffer_size = 1024;
            char* request_buffer = (char*)malloc(infile_read_size * request_buffer_size);
            while(infile.good() && !infile.eof() && load_index_ < num_) {
                infile.read(request_buffer, infile_read_size * request_buffer_size);
                if(infile.eof()){
                    for(int i = 0; i < file_request_num % request_buffer_size; i++){
                        int op_bin = *(int*)(request_buffer + i * infile_read_size + sizeof(int));
                        uint32_t hash_key = *(uint32_t*)(request_buffer + i * infile_read_size + sizeof(int) * 2);
                        requests_[load_index_] = request(uint8_op_map[op_bin], hash_key);
                        load_index_ ++;
                    }
                } else{
                    for(int i = 0; i < request_buffer_size; i++){
                        int op_bin = *(int*)(request_buffer + i * infile_read_size + sizeof(int));
                        uint32_t hash_key = *(uint32_t*)(request_buffer + i * infile_read_size + sizeof(int) * 2);
                        requests_[load_index_] = request(uint8_op_map[op_bin], hash_key);
                        load_index_ ++;
                    }
                }
            }
            infile.close();
            free(request_buffer);
        }

        queue_size = load_index_;

        std::cout << "origin data size :" << queue_size << std::endl;
        printf("\n");
        auto end = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double>(end - start).count();
        printf("loading workload finished in %.3lf s\n", time);
        fflush(stdout);
    }
    
    // Twitter original traces
    TraceLoader(const uint64_t num, std::string work_file, std::string work_type, std::string workload_start, std::string workload_end){
        if (! work_type.compare("Twitter")) {
            printf("using Twitter as test trace!\n");
        } else {
            printf("using unknown work type workload!\n");
            exit(0);
        }
        num_ = num;
        load_index_ = 0;
        printf("loading workload (%lu)...\n", num_);
        auto start = std::chrono::high_resolution_clock::now();
        std::string str;
        std::string op = "";
        std::ifstream infile;
        std::vector<std::string> suffixs;
        if(workload_start == "" || workload_end == "") {
            suffixs.push_back(work_file);
        } else {
            suffixs = extract_suffix_string(workload_start, workload_end);
        }

        requests_ = (request*)malloc(100ull*1024*1024*1024);
        if(requests_ == NULL) {
            printf("malloc failed!\n");
            exit(0);
        }

        for(auto suffix : suffixs){
            if (load_index_ >= num_) break;
            if(workload_start == "" || workload_end == ""){
                infile.open(suffix);
            } else {
                infile.open(work_file + "." + suffix);
                std::cout << "open file :" + work_file + "." + suffix << std::endl;
            }
            if( !infile.good()) {
                std::cout << "open file fail!" << std::endl;
                exit(0);
            }
            while(infile.good() && !infile.eof() && load_index_ < num_) {
                getline(infile, str);
                if(str == "") continue;
                std::string space_delimiter = ",";
                std::vector<std::string> words{};
                words.clear();
                size_t pos = 0;
                while ((pos = str.find(space_delimiter)) != std::string::npos) {
                    words.push_back(str.substr(0, pos));
                    str.erase(0, pos + space_delimiter.length());
                }
                words.push_back(str.substr(0, pos));
                //int value_size = strtol(words[3].c_str(), NULL, 10);
                op = words[5];
                unsigned int hashed_string = string_hash(words[1]);

                TraceLoader::Operation opt;
                if(op == "set"){
                    opt = TraceLoader::Operation::set;
                }
                else if(op =="add"){
                    opt = TraceLoader::Operation::add;
                }
                else if(op =="replace"){
                    opt = TraceLoader::Operation::replace;
                }
                else if(op =="append"){
                    opt = TraceLoader::Operation::append;
                }
                else if(op =="prepend"){
                    opt = TraceLoader::Operation::prepend;
                }
                else if(op =="cas"){
                    opt = TraceLoader::Operation::cas;
                }
                else if(op =="get"){
                    opt = TraceLoader::Operation::get;
                }
                else if(op =="gets"){
                    opt = TraceLoader::Operation::gets;
                }
                else if(op =="delete"){
                    opt = TraceLoader::Operation::delete_;
                }
                else if(op =="incr"){
                    opt = TraceLoader::Operation::incr;
                }
                else if(op =="decr"){
                    opt = TraceLoader::Operation::decr;
                }
                else{
                    std::cout << "error parameter" << op << std::endl;
                    continue;
                }
                requests_[load_index_] = request(opt, hashed_string);
                load_index_ ++;
            }
            infile.close();
        }

        // pre-fault
        // for(size_t i = 0; i < load_index_; i++){
        //     auto req = requests_[i];
        // }
        queue_size = load_index_;

        std::cout << "origin data size :" << queue_size << std::endl;
        printf("\n");
        auto end = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double>(end - start).count();
        printf("loading workload finished in %.3lf s\n", time);
        fflush(stdout);
    }


    request GetRequest(key_size_t index){
      return requests_[index % queue_size];
    }
    uint64_t GetQueueSize(){
        return queue_size;
    }
  private:
    uint64_t num_;
    uint64_t queue_size;
    request* requests_ = nullptr;
    uint64_t cursor_;
    uint64_t load_index_;
};

std::vector<std::string> extract_suffix_string(std::string const& start, std::string const& end) {
    std::vector<std::string> temp;
    int start_ = strtol(start.c_str(), NULL, 10);
    int end_ = strtol(end.c_str(), NULL, 10);
    for(int i = start_; i <= end_; i++) {
        std::string temp_str = std::to_string(i);
        temp.push_back(temp_str);
    }
    return temp;
}

unsigned int string_hash(std::string const& s){
    const int p = 31;
    const int m = 1e9 + 9;
    unsigned long hash_value = 0;
    unsigned long p_pow = 1;
    for (char c : s) {
        hash_value = (hash_value + (c - '0' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return hash_value;
}
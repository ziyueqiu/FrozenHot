#pragma once

#include "condition_variable"
#include "thread"
#include "atomic"
#include "assert.h"
#include "vector"
#include "mutex"

#include "spdk_log_writer.h"
#include "fs_log_writer.h"
#include "util.h"

namespace ssdlogging {

#define MAX_LOGS_NUM 50
#define DEFAULT_MAX_YIELD_USEC 0
#define DEFAULT_SLOW_YIELD_USEC 0

class LoggingServer{
	public:
		struct AdaptationContext;
		struct Writer;
		struct WriteGroup;

	private:
		int logs_num_;
		AbstractLogWriter **logs_;
		std::string log_writer_type_;
		std::string log_path_;


		uint64_t max_yield_usec_;
		uint64_t slow_yield_usec_;

		std::atomic<Writer*> newest_writer_;
		std::atomic<bool> log_free_[MAX_LOGS_NUM];

		std::thread server_;
		bool stop_;

		statistics::MyStat ssdlogging_time; // time spend in ssdlogging (nanos)

	public:
		LoggingServer(){
			printf("Defalut initialize logging \n");
		}

		LoggingServer(std::string log_writer_type, std::string log_path, int num):
				logs_num_(num),
				log_writer_type_(log_writer_type),
				log_path_(log_path),
				newest_writer_(nullptr),
				server_(std::bind(&LoggingServer::Server,this)),
				stop_(false){
			printf("Initialize logging server... \n");

			assert(logs_num_ < MAX_LOGS_NUM && logs_num_ > 0);

			logs_ = new AbstractLogWriter *[logs_num_];

			if(log_writer_type_ == "filesystem"){
				for(int i=0; i<logs_num_; i++){
					log_free_[i].store(false);
					logs_[i] = new FSLogWriter(log_path_, i);
				}
			}else if(log_writer_type_ == "spdk"){
				struct SPDKInfo *spdk = ssdlogging::InitSPDK(log_path_, logs_num_);
				for(int i=0; i<logs_num_;i++){
					log_free_[i].store(false);
					logs_[i] = new SPDKLogWriter(spdk, 1024*1024*i, i);
				}
			}else{
				assert(0);
			}
		}

		~LoggingServer(){
			PrintStatistics();
			for(int i=0; i<logs_num_;i++){
				delete logs_[i];
			}
			StopServer();
			printf("Delete logging server...\n");
		}

		void AddLog(const char *data, size_t size);

		void PrintStatistics();

		void ResetStat();

	private:

		void StopServer();

		uint8_t BlockingAwaitState(Writer* w, uint8_t goal_mask);

		uint8_t AwaitState(Writer* w, uint8_t goal_mask, AdaptationContext* ctx);

		void SetState(Writer* w, uint8_t new_state);

		void CreateMissingNewerLinks(Writer *head);

		int GetLeader();

		void AddToLink(Writer *w, std::atomic<Writer*>* newest_writer);

		void MakeGroup(Writer* leader, WriteGroup *write_group);

		void WriteToLog(Writer* leader, WriteGroup *write_group);

		void FinishGroup(Writer* leader, WriteGroup *write_group);

		void Server();

	public:
		enum State : uint8_t {
			STATE_INIT = 1,
			STATE_GROUP_LEADER = 2,
			STATE_COMPLETED = 4,
			STATE_LOCKED_WAITING = 8,
			STATE_START_WRITE = 16
		};	

		struct WriteGroup {
			Writer* leader = nullptr;
			Writer* last_writer = nullptr;
			size_t size = 0;
			struct Iterator {
				Writer* writer;
				Writer* last_writer;
				explicit Iterator(Writer* w, Writer* last)
					: writer(w), last_writer(last) {}
				Writer* operator*() const { return writer; }
				Iterator& operator++() {
					assert(writer != nullptr);
					if (writer == last_writer) {
						writer = nullptr;
					} else {
					writer = writer->link_newer;
					}
					return *this;
				}
				bool operator!=(const Iterator& other) const {
					return writer != other.writer;
				}
			};
			Iterator begin() const { return Iterator(leader, last_writer); }
			Iterator end() const { return Iterator(nullptr, nullptr); }
		};

		struct Writer {
			const char *data;
			size_t size;
			int log_num;
			Writer* link_older;
			Writer* link_newer;
			WriteGroup *write_group;
			std::atomic<uint8_t> state;
			bool made_waitable;
			std::aligned_storage<sizeof(std::mutex)>::type state_mutex_bytes;
			std::aligned_storage<sizeof(std::condition_variable)>::type state_cv_bytes;
			std::chrono::high_resolution_clock::time_point start_time;
			std::chrono::high_resolution_clock::time_point start_writing;
			std::chrono::high_resolution_clock::time_point leader_time;
			std::chrono::high_resolution_clock::time_point finish_time;
			bool flag;

			Writer(const char *_data, size_t _size):
				data(_data),
				size(_size),
				log_num(-1),
				link_older(nullptr),
				link_newer(nullptr),
				write_group(nullptr),
				state(STATE_INIT),
				made_waitable(false){ }

			 ~Writer() {
				if (made_waitable) {
					StateMutex().~mutex();
					StateCV().~condition_variable();
				}
			}
			void CreateMutex() {
				if (!made_waitable) {
					made_waitable = true;
					new (&state_mutex_bytes) std::mutex;
					new (&state_cv_bytes) std::condition_variable;
				}
			}
			std::mutex& StateMutex() {
				assert(made_waitable);
				return *static_cast<std::mutex*>(static_cast<void*>(&state_mutex_bytes));
			}
			std::condition_variable& StateCV() {
	 			assert(made_waitable);
	 			return *static_cast<std::condition_variable*>(static_cast<void*>(&state_cv_bytes));
			}
		};
		struct AdaptationContext {
	 		const char* name;
			std::atomic<int32_t> value;
			explicit AdaptationContext(const char* name0) : name(name0), value(0) {}
		};
};
}
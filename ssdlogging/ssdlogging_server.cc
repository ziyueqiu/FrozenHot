#include "ssdlogging_server.h"
#include "emmintrin.h"

namespace ssdlogging {

static LoggingServer::AdaptationContext jbg_ctx("AddLog");
void LoggingServer::AddLog(const char *data, size_t size){
	auto start = SSDLOGGING_TIME_NOW;

	assert(data != nullptr);
	Writer w(data, size);
	AddToLink(&w, &newest_writer_);
	AwaitState(&w, STATE_GROUP_LEADER | STATE_COMPLETED, &jbg_ctx);
	if(w.state == STATE_COMPLETED){
		double time = SSDLOGGING_TIME_DURATION(start, SSDLOGGING_TIME_NOW);
		ssdlogging_time.add(time);
		return ;
	}
	assert(w.state == LoggingServer::STATE_GROUP_LEADER);
	WriteGroup write_group;
	MakeGroup(&w, &write_group);
	WriteToLog(&w, &write_group);
	FinishGroup(&w, &write_group);

	double time = SSDLOGGING_TIME_DURATION(start, SSDLOGGING_TIME_NOW);
	ssdlogging_time.add(time);
}

void LoggingServer::CreateMissingNewerLinks(Writer* head) {
	while (true) {
		Writer* next = head->link_older;
		if (next == nullptr || next->link_newer != nullptr) {
			assert(next == nullptr || next->link_newer == head);
			break;
		}
		next->link_newer = head;
		head = next;
	}
}

void LoggingServer::Server(){
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(15,&mask);
	int rc = sched_setaffinity(syscall(__NR_gettid), sizeof(mask), &mask);
	assert( rc == 0);
	while(true){
		if(newest_writer_.load() != nullptr){
			int log_num = GetLeader();
			if(log_num >= 0){
				Writer* head = newest_writer_.exchange(nullptr, std::memory_order_acquire);
				if(head != nullptr){
					Writer *leader=head;
					leader->log_num = log_num;
					SetState(leader, STATE_GROUP_LEADER);
				}else{
					log_free_[log_num].store(false);
				}
			}
		}
		if(stop_)
			break;
	}
}

void LoggingServer::StopServer(){
	stop_ = true;
	server_.join();
}

int LoggingServer::GetLeader(){
	int target_log = -1;
	for(int i=0; i<logs_num_; i++){
		bool flag = false;
		if(log_free_[i].compare_exchange_weak(flag, true)){
			assert(log_free_[i].load());
			return i;
		}
	}
	return target_log;
}

void LoggingServer::AddToLink(Writer *w, std::atomic<Writer*>* newest_writer){
	assert(w != nullptr);
	assert(w->state.load() == STATE_INIT);
	Writer* writers = newest_writer->load(std::memory_order_relaxed);
	while (true) {
		w->link_older = writers;
		if (newest_writer->compare_exchange_weak(writers, w)) {
			return ;
		}
	}
}

void LoggingServer::MakeGroup(Writer* leader, WriteGroup *write_group){
	assert(leader->link_newer == nullptr);
	assert(log_free_[leader->log_num].load());
	assert(leader->log_num >= 0 && leader->log_num < logs_num_);

	//CreateMissingNewerLinks(leader);	

	leader->write_group = write_group;
	write_group->leader = leader;
	write_group->last_writer = leader;
	write_group->size = 1;

   	Writer* w = leader->link_older;
   	while(w != nullptr && w != leader){
   		w->write_group = write_group;
   		write_group->last_writer = w;
   		write_group->size++;
		w = w->link_older;
   	}
}

void LoggingServer::WriteToLog(Writer* leader, WriteGroup *write_group){
	//TODO
	assert(leader == write_group->leader);
	// int time = rand() % 200;
	// std::this_thread::sleep_for(std::chrono::microseconds(time));
	Writer *w = leader;
	while(w != nullptr) {
		logs_[leader->log_num]->AppendData(w->data,w->size);
		w = w->link_older;
	}
	logs_[leader->log_num]->Sync();
}

void LoggingServer::FinishGroup(Writer* leader, WriteGroup *write_group){
	assert(leader != nullptr);
	assert(leader == write_group->leader);
	assert(log_free_[leader->log_num].load());
	log_free_[leader->log_num].store(false);

	Writer *head = leader->link_older;

	while(head != nullptr){
		Writer *tmp = head->link_older; // save older
		SetState(head, STATE_COMPLETED);// free current
		//head->state.store(STATE_COMPLETED);
		//std::thread(&LoggingServer::SetState, this, head, STATE_COMPLETED).detach();
		head = tmp;
	}
}

uint8_t LoggingServer::AwaitState(Writer* w, uint8_t goal_mask, AdaptationContext* ctx){

	/*
	  1. Loop
	  2. Short-Wait: Loop + std::this_thread::yield()
	  3. Long-Wait: std::condition_variable::wait()
	*/
	/* 1. Busy loop using "pause" for 1 micro sec.
	   On a modern Xeon each loop takes about 7 nanoseconds (most of which
       is the effect of the pause instruction), so 200 iterations is a bit
       more than a microsecond.  This is long enough that waits longer than
       this can amortize the cost of accessing the clock and yielding.
    */
	uint8_t state;
	for (uint32_t tries = 0; tries < 200; ++tries) {
		state = w->state.load(std::memory_order_acquire);
		if ((state & goal_mask) != 0) {
			return state;
		}
		port::AsmVolatilePause();
	}
	/*
	  2. Short wait.
	*/
	const size_t kMaxSlowYieldsWhileSpinning = 3;
 	auto& yield_credit = ctx->value;
	bool update_ctx = false;
	bool would_spin_again = false;
	const int sampling_base = 256;
	if (max_yield_usec_ > 0) {
		update_ctx = random::Random::GetTLSInstance()->OneIn(sampling_base);

		if (update_ctx || yield_credit.load(std::memory_order_relaxed) >= 0) {
			auto spin_begin = std::chrono::steady_clock::now();
			size_t slow_yield_count = 0;

			auto iter_begin = spin_begin;
			while ((iter_begin - spin_begin) <= std::chrono::microseconds(max_yield_usec_)) {
				std::this_thread::yield();

				state = w->state.load(std::memory_order_acquire);
				if ((state & goal_mask) != 0) {
					would_spin_again = true;
			 		break;
				}
				auto now = std::chrono::steady_clock::now();
				if (now == iter_begin ||
					now - iter_begin >= std::chrono::microseconds(slow_yield_usec_)) {
					++slow_yield_count;
					if (slow_yield_count >= kMaxSlowYieldsWhileSpinning) {
						update_ctx = true;
						break;
					}
				}
				iter_begin = now;
			}
		}
	}
	/*
	  3. Long wait.
	*/
	if ((state & goal_mask) == 0) {
		w->CreateMutex();
 		state = w->state.load(std::memory_order_acquire);
		assert(state != STATE_LOCKED_WAITING);
		if ((state & goal_mask) == 0 &&
			w->state.compare_exchange_strong(state, STATE_LOCKED_WAITING)) {
			// we have permission (and an obligation) to use StateMutex
			std::unique_lock<std::mutex> guard(w->StateMutex());
			w->StateCV().wait(guard, [w] {
				return w->state.load(std::memory_order_relaxed) != STATE_LOCKED_WAITING;
			});
			state = w->state.load(std::memory_order_relaxed);
		}
		assert((state & goal_mask) != 0);
	}
	// update yield_credit
	if (update_ctx) {
		auto v = yield_credit.load(std::memory_order_relaxed);
		v = v - (v / 1024) + (would_spin_again ? 1 : -1) * 131072;
		yield_credit.store(v, std::memory_order_relaxed);
	}
  	assert((state & goal_mask) != 0);
  	return state;
}

void LoggingServer::SetState(Writer* w, uint8_t new_state){

	auto state = w->state.load(std::memory_order_acquire);
	assert(state != STATE_COMPLETED);
	if (state == STATE_LOCKED_WAITING ||
      !w->state.compare_exchange_strong(state, new_state)) {
		assert(state == STATE_LOCKED_WAITING);
		std::lock_guard<std::mutex> guard(w->StateMutex());
		assert(w->state.load(std::memory_order_relaxed) != new_state);
		w->state.store(new_state, std::memory_order_relaxed);
		w->StateCV().notify_one();
	}
}

void LoggingServer::ResetStat(){
	ssdlogging_time.reset();
	for(int i=0; i<logs_num_;i++){
		logs_[i]->ResetStat();
	}
}

void LoggingServer::PrintStatistics(){
	double sync_time = 0.0;
	uint64_t sync_num = 0;
	uint64_t written_size = 0;
	for(int i=0; i <logs_num_; i++){
		sync_time += logs_[i]->GetSyncTime();
		sync_num += logs_[i]->GetSyncNum();
		written_size += logs_[i]->GetWrittenSize();
	}
	printf("-----------SSDLogging Performance-------------\n");
	ssdlogging_time.print_data("time in ssdlogging");
	printf("sync time: %lf\n",sync_time);
	printf("sync num: %ld\n", sync_num);
	printf("sync latency: %lf\n", sync_time/sync_num);
	printf("logging batch size: %lf\n", ssdlogging_time.size()*1.0/sync_num);
	printf("written size: %ld\n", written_size);
	printf("-----------SSDLogging Performance-------------\n");
}

}
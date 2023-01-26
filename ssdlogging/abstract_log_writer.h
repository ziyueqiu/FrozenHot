#pragma once

#include "stdint.h"
#include "string"
#include "fcntl.h"
#include "unistd.h"
#include "chrono"
#include "cstring"
#include "assert.h"
#include "util.h"

namespace ssdlogging {

#define MAX_BUFFER_SIZE (1024*1024)

/*
* Abstract log writer
*/
class AbstractLogWriter{
	protected:
		statistics::MyStat sync_time;
		uint64_t total_written_size;
		uint64_t current_buffer_offset;
		int log_num;
		char *buffer;

	public:

		AbstractLogWriter(int _log_num):
			total_written_size(0),
			current_buffer_offset(0),
			log_num(_log_num),
			buffer(nullptr){}

		virtual ~AbstractLogWriter() { };

		virtual void AppendData(const char *data, size_t size) = 0;

		virtual void Sync() = 0;

		double GetSyncTime(){
			return sync_time.sum();
		}

		uint64_t GetSyncNum(){
			return sync_time.size();
		}

		uint64_t GetWrittenSize(){
			return total_written_size;
		}

		void ResetStat(){
			sync_time.reset();
			total_written_size = 0;
		}
};

}
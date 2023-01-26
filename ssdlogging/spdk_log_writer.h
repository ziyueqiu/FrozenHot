#pragma once

#include "stdint.h"
#include "string"
#include "fcntl.h"
#include "unistd.h"
#include "chrono"
#include "cstring"
#include "assert.h"

#include "spdk_device.h"
#include "abstract_log_writer.h"

namespace ssdlogging {

/*
* write log data with spdk
*/
class SPDKLogWriter : public AbstractLogWriter{
	private:
		const int write_align = 4096;
		struct SPDKInfo *spdk;
		uint64_t start_offset;
		uint64_t current_offset;

	public:
		SPDKLogWriter(struct SPDKInfo *_spdk, uint64_t _start_offset, int _log_num):
				AbstractLogWriter(_log_num),
				spdk(_spdk),
				start_offset(_start_offset),
				current_offset(_start_offset){
			// spdk_zmalloc(size_t size, size_t align, uint64_t* phys_addr, int socket_id, uint32_t flags)
			buffer = (char*)spdk_zmalloc(MAX_BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
		}

		~SPDKLogWriter(){
			if(buffer != nullptr)
				spdk_free(buffer);
		}

		void AppendData(const char *data, size_t size);

		void Sync();

		void test();
};
}
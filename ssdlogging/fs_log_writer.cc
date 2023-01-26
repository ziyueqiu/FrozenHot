#include "fs_log_writer.h"
 
namespace ssdlogging {

void FSLogWriter::AppendData(const char *data, size_t size){
	assert(current_buffer_offset + size < MAX_BUFFER_SIZE);

	memcpy(buffer + current_buffer_offset, data, size);
	current_buffer_offset += size;
}

void FSLogWriter::Sync(){
	auto start = SSDLOGGING_TIME_NOW;

	size_t sz = write(fd, buffer, current_buffer_offset);
	assert(current_buffer_offset == sz);
	int rc = fdatasync(fd);
	assert(rc == 0);

	double time = SSDLOGGING_TIME_DURATION(start, SSDLOGGING_TIME_NOW);
	sync_time.add(time);

	total_written_size += current_buffer_offset;
	current_buffer_offset = 0;
}

}
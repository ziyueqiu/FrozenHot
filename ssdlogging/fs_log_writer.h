#pragma once

#include "stdint.h"
#include "string"
#include "fcntl.h"
#include "unistd.h"
#include "chrono"
#include "cstring"
#include "assert.h"
#include "abstract_log_writer.h"

namespace ssdlogging {

/*
* write log data on a file system
*/
class FSLogWriter : public AbstractLogWriter{
	private:
		int fd;
		std::string directory;
		std::string filename;

	public:
		FSLogWriter(std::string _directory, int _log_num):
				AbstractLogWriter(_log_num),
				directory(_directory){
			buffer = (char *) malloc(sizeof(char) * MAX_BUFFER_SIZE);
			assert(buffer != nullptr);
			filename = directory + "/log-" + std::to_string(_log_num);
			fd  = open(filename.c_str(), O_RDWR | O_CREAT, 00644);
			assert(fd > 0);
		}

		void AppendData(const char *data, size_t size);

		void Sync();

		~FSLogWriter(){
			if(buffer != nullptr)
				free(buffer);
		}
};
}
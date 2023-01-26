#include "spdk_log_writer.h"
 
namespace ssdlogging {

void SPDKLogWriter::AppendData(const char *data, size_t size){
	assert(current_buffer_offset + size < MAX_BUFFER_SIZE);

	memcpy(buffer + current_buffer_offset, data, size);
	current_buffer_offset += size;
}

void SPDKLogWriter::Sync(){
	auto start = SSDLOGGING_TIME_NOW;

	struct NSEntry *ns_entry = spdk->namespaces; //use namespace 0 by default
	assert(ns_entry != nullptr);
	if(current_buffer_offset % write_align != 0){
		current_buffer_offset = (current_buffer_offset/write_align + 1)*write_align;
	}
	int sector_num = current_buffer_offset / ns_entry->sector_size;
	bool is_completed = false;
	//printf("write %d sector starting from %lld\n", sector_num, current_offset);
	//printf("%lld\n", buffer);
	int rc = spdk_nvme_ns_cmd_write(ns_entry->ns,
						ns_entry->qpair[log_num],
						buffer,
					    current_offset,
					    sector_num,
					    [](void *arg, const struct spdk_nvme_cpl *completion) -> void {
					    	bool *finished = (bool *)arg;
					    	if (spdk_nvme_cpl_is_error(completion)) {
								fprintf(stderr, "I/O error status: %s\n", 
									spdk_nvme_cpl_get_status_string(&completion->status));
								fprintf(stderr, "Write I/O failed, aborting run\n");
								*finished = true;
								exit(1);
							}
							*finished = true;
					    },
					    &is_completed, 
					    0);
	if(rc != 0){
		printf("spdk write failed: %d\n", rc*-1);
		assert(0);
	}
	while (!is_completed) {
		spdk_nvme_qpair_process_completions(ns_entry->qpair[log_num], 0);
	}

	double time = SSDLOGGING_TIME_DURATION(start, SSDLOGGING_TIME_NOW);
	sync_time.add(time);

	current_offset += sector_num;
	total_written_size += current_buffer_offset;
	current_buffer_offset = 0;

}

void SPDKLogWriter::test(){
	char data[] = "hello world";
	int num = 2000;
	auto start = std::chrono::high_resolution_clock::now();
	for(int i=0; i<num; i++){
		AppendData(data, sizeof(data));
		Sync();
	}
	auto end = std::chrono::high_resolution_clock::now();
	double time = std::chrono::duration<double>(end - start).count() * 1000;
	printf("average: %lf\n", time/num*1000);
	printf("average: %lf\n", GetSyncTime()/num*1000);
}

}
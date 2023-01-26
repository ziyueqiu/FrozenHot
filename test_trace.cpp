#include "ssdlogging/util.h"
#include "traceloader/client.h"
#include <functional>
#include <thread>
#include "ssdlogging/properties.h"
void ParseCommandLine(int argc, const char *argv[], utils::Properties &props);

int main(const int argc, const char *argv[]){
	utils::Properties props;
	ParseCommandLine(argc, argv, props);
    printf("Warmup\n");
    auto client = new Client(props);
    client->warmup();
	delete client;
}

void ParseCommandLine(int argc, const char *argv[], utils::Properties &props) {
	// <threads num> <cache size> <request num> <seg num> <workload type>
    // <workload file or Zipf const> <cache type> <disk lat>
	if(argc != 10 && argc != 12){
		printf("argc = %d\n", argc);
		printf("usage:<threads num> <cache size> <request num> <seg num> <workload type> <workload file prefix or Zipf const> <cache type> <disk lat> [workload start] [workload end]\n");
		exit(0);
	}
	std::ifstream input(argv[1]);

	//1. threads number
	props.SetProperty("threads num", argv[1]);

	//2. cache size
	props.SetProperty("cache size", argv[2]);

	//3. request num
	props.SetProperty("request num", argv[3]);

	//4. seg num
	props.SetProperty("seg num", argv[4]);

	//5. workload type
	props.SetProperty("workload type", argv[5]);

	//6. workload file 1
	props.SetProperty("workload file or zipf const", argv[6]);

	//7. cache type
	props.SetProperty("cache type", argv[7]);

	//8. disk lat
	props.SetProperty("disk lat", argv[8]);

	//9. rebuild frequency (0 for baseline, useless)
	props.SetProperty("rebuild freq", argv[9]);

	if(argc == 12) {
		props.SetProperty("workload start", argv[10]);
		props.SetProperty("workload end", argv[11]);
	}
}
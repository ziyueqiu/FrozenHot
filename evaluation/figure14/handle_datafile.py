
import re
import os
import csv

g = os.walk("../origin_data/figure14")
order = ["trace", "thread", "algo_type", "disk lat", "refresh ratio", "cachesize", "thput-b", "total miss ratio", "hit latency"]
with open("figure.csv", "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(order)
    for path, dir_list, file_list in g:
        for file_name in file_list:
            if not file_name.endswith(".txt"):
                continue
            print(file_name)
            path_ = os.path.join(path, file_name)
            res_file = open(path_, 'r')
            lines = res_file.readlines()
            max_step = 0
            data_flag = False
            for line in lines:
                if re.match("data pass \d+", line):
                    data_flag = True
                elif data_flag and re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, miss ratio: \d+\.\d+(.)*", line):
                    data_flag = False
                    flag = re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, miss ratio: \d+\.\d+(.)*", line)
                    handled_request_size = int(flag.group(0).split(' ')[5].replace(',', ''))
                    max_step += handled_request_size
            res = {}
            data_flag = False
            res['trace'] = "Zipf" if file_name.find("Zipf") != -1 else "Twitter" if file_name.find("MSR") == -1 else "MSR"
            if res['trace'] == "Twitter":
                res['trace'] += " " + file_name.split("_")[5]
            elif res['trace'] == "MSR":
                res['trace'] += " " + file_name.split("_")[5] + "_" + file_name.split("_")[6]
            thread_num = int(file_name.split("_")[3].replace("thd", ""))
            disk_laten = int(file_name.split("_")[2].replace("u", ""))
            refresh_ratio = "no FH" if file_name.find("FH") == -1 else "0" if file_name.find("rebuild10000") != -1 else file_name.split("rebuild")[1].replace('.txt', '')
            if file_name.find("no_rebuild") != -1:
                refresh_ratio = "0"
            res['thread'] = thread_num
            res['disk lat'] = disk_laten
            res['refresh ratio'] = refresh_ratio
            if file_name.find("LRU_FH") != -1:
                res['algo_type'] = "LRU FH"
            elif file_name.find("LFU_FH") != -1:
                res['algo_type'] = "LFU FH"
            elif file_name.find("LFU") != -1:
                res['algo_type'] = "LFU"
            elif file_name.find("FIFO_FH") != -1:
                res['algo_type'] = "FIFO FH"
            elif file_name.find("FIFO") != -1:
                res['algo_type'] = "FIFO"
            elif file_name.find("Redis_LRU") != -1:
                res['algo_type'] = "Redis LRU"
            elif file_name.find("StrictLRU") != -1:
                res["algo_type"] = "Strict LRU"
            else:
                res['algo_type'] = "LRU"
            res['cachesize'] = (file_name.split("size")[0].split("_")[-1])
            # res['cachesize'] = "small" if file_name.find("5000") != -1 else "large"
            wait_stable_time = 0
            useless_time = 0
            useless_req = 0
            max_ = 0
            now_step = 0
            final_flag = False
            for line in lines:
                if re.match("wait stable spend time: \d+\.\d+ s", line):
                    flag = re.match("wait stable spend time: \d+\.\d+ s", line)
                    wait_stable_time = float(flag.group(0).split(' ')[4])
                elif re.match("data pass \d+", line):
                    flag = re.match("data pass \d+", line)
                    pass_ = int(flag.group(0).split(' ')[2])
                    if pass_ == 0:
                        continue
                    data_flag = True
                elif data_flag and re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, duration: \d+\.\d+ s(.)*", line):
                    data_flag = False
                    flag = re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, duration: \d+\.\d+ s(.)*", line)
                    handled_request_size = int(flag.group(0).split(' ')[5].replace(',', ''))
                    duration_time = float(flag.group(0).split(' ')[7])
                    now_step += handled_request_size
                    if now_step >= max_step * 0.98 and thread_num != 1:
                        useless_req += handled_request_size
                        useless_time += duration_time
                        continue
                elif re.match("All threads run \d+\.\d+ s", line):
                    flag = re.match("All threads run \d+\.\d+ s", line)
                    data_flag = True
                    final_flag = True
                    all_thread_run_time = float(flag.group(0).split(' ')[3])
                elif re.match("granularity: \d+ and large granularity: \d+", line):
                    flag = re.match("granularity: \d+ and large granularity: \d+", line)
                    large_gran = int(flag.group(0).split(' ')[5])
                elif final_flag and re.match("- Hit Avg: \d+\.\d+ (.)*", line):
                    flag = re.match("- Hit Avg: \d+\.\d+ (.)*", line)
                    res["hit latency"] = float(flag.group(0).split(" ")[3])
                elif data_flag and re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, miss ratio: \d+\.\d+(.)*", line):
                    data_flag = False
                    flag = re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, miss ratio: \d+\.\d+(.)*", line)
                    total_size = int(flag.group(0).split(' ')[5].replace(',', ''))
                    res["total miss ratio"] = float(flag.group(0).split(' ')[8].replace(')', ''))
                    print(wait_stable_time, all_thread_run_time)
                    print(useless_time)
                    res["thput-b"] = (total_size - useless_req) * large_gran * 1.0 / (all_thread_run_time - wait_stable_time - useless_time)
            li = []
            for item in order:
                if item not in res:
                    li.append("")
                else:
                    li.append(res[item])
            writer.writerow(li)
            res = {}
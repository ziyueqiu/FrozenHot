import re
import os
import csv

g = os.walk("../origin_data/figure10")
order = ["step", "hit ratio", "hit laten", "th-put-a", "th-put-b", "type", "episode"]
global_order = ["step", "global ratio", "hit laten", "th-put-a", "th-put-b", "type", "episode"]
fh_oreder = ["step", "fh ratio", "hit laten", "th-put-a", "th-put-b", "type", "episode"]
with open("evaluation-update.csv", "w", newline="") as csvfile:
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
            max_ = 0
            thread_num = int(file_name.split("_")[3].replace("thd", ""))
            if file_name.find("StrictLRU") != -1:
                continue
            if file_name.find("LRU_FH") != -1:
                res_type = "LRU FH"
            elif file_name.find("Redis_LRU") != -1:
                res_type = "Redis LRU"
            elif file_name.find("LFU_FH") != -1:
                res_type = "LFU FH"
            elif file_name.find("LFU") != -1:
                res_type = "LFU"
            else:
                res_type = "LRU"
            data_flag = False
            now_step = 0
            episode = ""
            best_sleep_ratio = 0
            large_gran = 0
            wait_stable_time = 0
            all_thread_run_time = 0
            useless_time = 0
            shard = int(file_name.split("_")[1].replace("shard", ""))
            useless_req = 0
            wait_stable_req = 0
            for line in lines:
                if re.match("\* start observation \*", line):
                    episode = "baseline"
                elif re.match("\* start search \*", line) or re.match("\* start construct \*", line):
                    episode = "learning"
                elif re.match("\* start frozen \*", line):
                    episode = "frozen"
                elif re.match("granularity: \d+ and large granularity: \d+", line):
                    flag = re.match("granularity: \d+ and large granularity: \d+", line)
                    large_gran = int(flag.group(0).split(' ')[5])
                elif re.match("wait stable spend time: \d+\.\d+ s", line):
                    flag = re.match("wait stable spend time: \d+\.\d+ s", line)
                    wait_stable_time = float(flag.group(0).split(' ')[4])
                elif re.match("fail \d+ shard", line):
                    flag = re.match("fail \d+ shard", line)
                    failed_shard = int(flag.group(0).split(" ")[1])
                elif re.match("data pass \d+", line):
                    flag = re.match("data pass \d+", line)
                    pass_ = int(flag.group(0).split(' ')[2])
                    if pass_ == 0:
                        continue
                    res["type"] = res_type
                    res["episode"] = episode
                    data_flag = True
                elif re.match("best sleep ratio is found: \d+\.\d+", line):
                    flag = re.match("best sleep ratio is found: \d+\.\d+", line)
                    best_sleep_ratio = float(flag.group(0).split(' ')[5])
                elif re.match("\(Update\) best avg: \d+\.\d+ us, best size: \d+\.\d+(.)*", line):
                    flag = re.match("\(Update\) best avg: \d+\.\d+ us, best size: \d+\.\d+(.)*", line)
                    best_sleep_ratio = float(flag.group(0).split(' ')[7])
                elif re.match("All threads run \d+\.\d+ s", line):
                    flag = re.match("All threads run \d+\.\d+ s", line)
                    all_thread_run_time = float(flag.group(0).split(' ')[3])
                    data_flag = True
                    if res_type == "LRU":
                        res["type"] = "total LRU"
                    elif res_type == "LFU LRU":
                        res["type"] = "total LFU LRU"
                    elif res_type == "Redis LRU":
                        res["type"] = "total Redis LRU"
                    elif res_type == "LFU FH":
                        res["type"] = "total LFU FH"
                    else:
                        res["type"] = "total LRU FH"
                elif data_flag and episode == "frozen" and re.match(
                        "Total miss rate: \d+\.\d+ / \d+\.\d+, fast find hit: \d+, global hit: \d+, global miss: \d+",
                        line):
                    flag = re.match(
                        "Total miss rate: \d+\.\d+ / \d+\.\d+, fast find hit: \d+, global hit: \d+, global miss: \d+",
                        line)
                    res["global ratio"] = 1 - float(flag.group(0).split(' ')[5].replace(',', ''))
                    res["fh ratio"] = 1 - float(flag.group(0).split(' ')[3].replace(',', ''))
                elif data_flag and re.match("Total miss rate: \d+\.\d+, hit num: \d+, miss num: \d+",
                                            line) and (res_type == "LRU" or res_type == "Redis LRU" or res_type == "LFU"):
                    flag = re.match("Total miss rate: \d+\.\d+, hit num: \d+, miss num: \d+", line)
                    res["hit ratio"] = 1 - float(flag.group(0).split(' ')[3].replace(',', ''))
                elif data_flag and re.match("Total Avg Lat: -nan(.)*", line):
                    data_flag = False
                elif data_flag and re.match("Total miss rate: \d+\.\d+, hit num: \d+, miss num: \d+",
                                            line) and (res_type == "LRU FH" or res_type == "LFU FH"):
                    flag = re.match("Total miss rate: \d+\.\d+, hit num: \d+, miss num: \d+", line)
                    res["global ratio"] = 1 - float(flag.group(0).split(' ')[3].replace(',', ''))
                    res["fh ratio"] = res["global ratio"]
                elif data_flag and re.match("- Hit Avg: \d+\.\d+ \(stat size: \d+, real size_: \d+\)(.)*", line):
                    flag = re.match("- Hit Avg: \d+\.\d+ \(stat size: \d+, real size_: \d+\)(.)*", line)
                    res["hit laten"] = flag.group(0).split(' ')[3]
                elif data_flag and re.match("- Hit Avg: \d+\.\d+ \(stat size: \d+, size: \d+ -> \d+\)(.)*", line):
                    flag = re.match("- Hit Avg: \d+\.\d+ \(stat size: \d+, size: \d+ -> \d+\)(.)*", line)
                    res["hit laten"] = flag.group(0).split(' ')[3]
                elif data_flag and re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, miss ratio: \d+\.\d+(.)*", line):
                    data_flag = False
                    flag = re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, miss ratio: \d+\.\d+(.)*", line)
                    request_avg = float(flag.group(0).split(' ')[3])
                    total_size = int(flag.group(0).split(' ')[5].replace(',', ''))
                    res["th-put-a"] = thread_num / request_avg * 1000 * 1000
                    print(wait_stable_time, all_thread_run_time)
                    res["th-put-b"] = (total_size - useless_req) * large_gran * 1.0 / (all_thread_run_time - wait_stable_time - useless_time)
                    res["hit ratio"] = 1 - float(flag.group(0).split(' ')[8].replace(")", ""))
                    res["episode"] = best_sleep_ratio
                    li = []
                    for item in order:
                        if item not in res:
                            li.append("")
                        else:
                            li.append(res[item])
                    writer.writerow(li)
                    res = {}
                elif data_flag and re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, duration: \d+\.\d+ s(.)*", line):
                    data_flag = False
                    flag = re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, duration: \d+\.\d+ s(.)*", line)
                    handled_request_size = int(flag.group(0).split(' ')[5].replace(',', ''))
                    now_step += handled_request_size
                    res["step"] = now_step
                    duration_time = float(flag.group(0).split(' ')[7])
                    request_avg = float(flag.group(0).split(' ')[3])
                    max_ = max(max_, handled_request_size / duration_time)
                    if now_step >= max_step * 0.80:
                        useless_req += handled_request_size
                        useless_time += duration_time
                        continue
                    else:
                        res["th-put-a"] = thread_num / request_avg * 1000 * 1000
                        res["th-put-b"] = handled_request_size * large_gran / duration_time
                        if res_type == "LRU FH" or res_type == "LFU FH":
                            li = []
                            res["type"] = "global LRU FH" if res_type == "LRU FH" else "global LFU FH"
                            continue_flag = False
                            for item in global_order:
                                if item not in res:
                                    continue_flag = True
                                    break
                                else:
                                    li.append(res[item])
                            if continue_flag:
                                continue
                            writer.writerow(li)
                            li = []
                            res["type"] = "LRU FH" if res_type == "LRU FH" else "LFU FH"
                            continue_flag = False
                            for item in fh_oreder:
                                if item not in res:
                                    continue_flag = True
                                    break
                                else:
                                    li.append(res[item])
                            if continue_flag:
                                continue
                            writer.writerow(li)
                            res = {}
                        else:
                            li = []
                            continue_flag = False
                            for item in order:
                                if item not in res:
                                    continue_flag = True
                                    break
                                else:
                                    li.append(res[item])
                            if continue_flag:
                                continue
                            writer.writerow(li)
                            res = {}
        writer.writerow(['', '', '', '', failed_shard, "shard", shard])

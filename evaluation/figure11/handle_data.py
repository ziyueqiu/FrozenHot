import re
import os
import csv

g = os.walk("../origin_data/figure11")
# fc hit: request hit during frozen time / total req
# fc req: request handled during frozen time / total req
# hit_on_fc: fc hit / (fc hit + dc hit)
order = ["trace", "thread", "thput", "hit lat", "miss lat", "hit ratio", "algo type", "fc hit", "fc req", "hit on fc"]
with open("all.csv", "w", newline="") as csvfile:
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
            res = {}
            if file_name.find("Redis") != -1:
                res['algo type'] = "Redis"
            elif file_name.find("StrictLRU") != -1:
                res['algo type'] = "Strict LRU"
            elif file_name.find("LRU_FH") != -1:
                res['algo type'] = "LRU FH"
            elif file_name.find("LFU_FH") != -1:
                res["algo type"] = "LFU FH"
            elif file_name.find("LFU") != -1:
                res["algo type"] = "LFU"
            elif file_name.find("FIFO_FH") != -1:
                res["algo type"] = "FIFO FH"
            elif file_name.find("FIFO") != -1:
                res["algo type"] = "FIFO"
            else:
                res['algo type'] = "LRU"
            if res["algo type"] == "LRU FH" and file_name.find("rebuild20") != -1:
                res["algo type"] += "(20)"
            if res["algo type"] == "LRU FH" and file_name.find("rebuild100") != -1:
                res["algo type"] += "(100)"
            if res["algo type"] == "LRU FH" and file_name.find("rebuild10.txt") != -1:
                res["algo type"] += "(10)"
            if file_name.find("LO") != -1:
                res['algo type'] += "(LO)"
            if file_name.find("TBB") != -1:
                res['algo type'] += "(LO)"
            size = ""
            trace = ""
            if file_name.find("smallsize") != -1:
                size = "small"
            else:
                size = "large"
            if file_name.find("zipf") != -1:
                trace = "zipf"
            elif file_name.find("MSR") != -1:
                trace = file_name.split("_")[5] + "_" + file_name.split("_")[6]
            elif file_name.find("Twitter") != -1:
                trace = file_name.split("_")[5]
            thread_num = int(file_name.split('_')[3].replace("thd", ""))
            res["thread"] = thread_num
            res['trace'] = trace
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
            large_gran = 0
            wait_stable_flag = False
            wait_stable_req = 0
            now_step = 0
            useless_req = 0
            frozen_flag = False
            fc_hit_ratio = 0
            global_hit_ratio = 0
            fc_hit_req = 0
            dc_hit_req = 0
            fc_handled_req = 0
            total_hit_req = 0
            
            fc_hit = 0
            dc_hit = 0

            handled_request_size = 0
            total_handle_request_size = 0
            data_flag = False
            final_flag = False

            draw_curve_flag = False
            construct_flag = False
            construct_step = 0
            first_profiling = True
            for line in lines:
                if re.match("granularity: \d+ and large granularity: \d+", line):
                    flag = re.match("granularity: \d+ and large granularity: \d+", line)
                    large_gran = int(flag.group(0).split(' ')[5])
                elif re.match("data pass \d+", line):
                    flag = re.match("data pass \d+", line)
                    pass_ = int(flag.group(0).split(' ')[2])
                    if pass_ == 0:
                        continue
                    data_flag = True
                elif re.match("construct step: \d+", line):
                    flag = re.match("construct step: \d+", line)
                    construct_step = flag.group(0).split(" ")[2]
                    if not first_profiling:
                        total_handle_request_size += float(construct_step)
                    construct_flag = False
                elif re.match("\* start frozen \*", line):
                    frozen_flag = True
                elif re.match("\* end frozen \*", line):
                    frozen_flag = False
                elif re.match("\* start construct \*", line):
                    construct_flag = True
                    frozen_flag = False
                elif re.match("\* start observation \*", line):
                    frozen_flag = False
                elif re.match("\* start search \*", line):
                    frozen_flag = False
                elif frozen_flag and re.match("Total miss rate: \d+\.\d+ / \d+\.\d+, fast find hit: \d+, (.)*", line):
                    flag = re.match("Total miss rate: \d+\.\d+ / \d+\.\d+, fast find hit: \d+, (.)*", line)
                    fc_hit_ratio = 1 - float(flag.group(0).split(' ')[3])
                    global_hit_ratio = 1 - float(flag.group(0).split(' ')[5].replace(",", ""))
                    dc_hit += int(flag.group(0).split(" ")[12].replace(",", ""))
                    fc_hit += int(flag.group(0).split(" ")[9].replace(",", ""))
                elif data_flag and re.match("Total miss rate: \d+\.\d+, hit num: \d+, miss num: \d+", line):
                    flag = re.match("Total miss rate: \d+\.\d+, hit num: \d+, miss num: \d+", line)
                    global_hit_ratio = 1 - float(flag.group(0).split(' ')[3].replace(",", ""))
                    dc_hit += int(flag.group(0).split(" ")[6].replace(",", ""))
                elif data_flag and re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, duration: \d+\.\d+ s(.)*", line):
                    data_flag = False
                    flag = re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, duration: \d+\.\d+ s(.)*", line)
                    real_fc_hit_ratio = 1 - float(flag.group(0).split(' ')[12].replace(')', ''))
                    handled_request_size = int(flag.group(0).split(' ')[5].replace(',', ''))
                    if not construct_flag:
                        total_handle_request_size += handled_request_size
                    now_step += handled_request_size
                    if now_step >= max_step * 0.95 or draw_curve_flag:
                        if draw_curve_flag:
                            draw_curve_flag = False
                        useless_req += handled_request_size
                        continue
                    if frozen_flag:
                        dc_hit_req += float(handled_request_size * (global_hit_ratio - fc_hit_ratio))
                    else:
                        dc_hit_req += float(handled_request_size * global_hit_ratio)
                    if frozen_flag:
                        fc_hit_req += float(handled_request_size * fc_hit_ratio)
                        fc_handled_req += float(handled_request_size)
                elif re.match("the first wait stable", line):
                    wait_stable_flag = True
                elif wait_stable_flag and re.match("Total Avg Lat: \d+\.\d+ \(size: \d+,(.)*", line):
                    flag = re.match("Total Avg Lat: \d+\.\d+ \(size: \d+,(.)*", line)
                    wait_stable_req += int(flag.group(0).split(' ')[5].replace(',', ''))
                    wait_stable_flag = False
                elif re.match("All threads run \d+\.\d+ s", line):
                    flag = re.match("All threads run \d+\.\d+ s", line)
                    final_flag = True
                elif re.match("- Hit Avg: \d+\.\d+ (.)*", line) and final_flag:
                    flag = re.match("- Hit Avg: \d+\.\d+ (.)*", line)
                    res["hit lat"] = float(flag.group(0).split(' ')[3])
                elif re.match("- Other Avg: \d+\.\d+ (.)*", line) and final_flag:
                    flag = re.match("- Other Avg: \d+\.\d+ (.)*", line)
                    res["miss lat"] = float(flag.group(0).split(' ')[3])
                elif re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, miss ratio: \d+\.\d+\)", line) and final_flag:
                    flag = re.match("Total Avg Lat: \d+\.\d+ \(size: \d+, miss ratio: \d+\.\d+\)", line)
                    handled_request_size = int(flag.group(0).split(' ')[5].replace(',', ''))
                    total_hit_req = float(flag.group(0).split(" ")[5].replace(",", "")) * (1 - float(flag.group(0).split(' ')[8].replace(')', "")))
                    res["hit ratio"] = 1 - float(flag.group(0).split(' ')[8].replace(')', ''))
                    request_avg = float(flag.group(0).split(' ')[3])
                    if res["algo type"] == "LRU FH" or res["algo type"] == "LFU FH" or res["algo type"] == "LRU FH(20)" or res["algo type"] == "LRU FH(100)" or res["algo type"] == "LRU FH(1)" or res['algo type'] == "FIFO FH" or res["algo type"] == "FIFO FH(LO)" or res["algo type"] == "LFU FH(LO)" or res["algo type"] == "LRU FH(LO)":
                        res["fc hit"] = float(fc_hit_req) / float(total_hit_req)
                        res["fc req"] = float(fc_handled_req) / float(total_handle_request_size)
                        res["hit on fc"] = fc_hit_req / (fc_hit_req + dc_hit_req)
                        print("final")
                        print(fc_hit_req, dc_hit_req)
                    res["thput"] = thread_num / request_avg * 1000 * 1000
            li = []
            for item in order:
                if item not in res:
                    li.append("")
                else:
                    li.append(res[item])
            writer.writerow(li)
            res = {}
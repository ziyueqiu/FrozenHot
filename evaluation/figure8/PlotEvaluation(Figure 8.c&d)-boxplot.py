import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def get_data_from_csv_by_algo_trace_thread(algo: str, trace: str, thread):
    df = pd.read_csv("fig3.csv")
    df = df[df["thread"] == str(thread)]
    df = df[df["algo type"] == algo]
    trace_list_ = []
    if trace == "Twitter":
        trace_list_ = twitter_list
    elif trace == "MSR":
        trace_list_ = msr_list
    else:
        return
    li = []
    for trace in trace_list_:
        tmp = df[df["trace"] == trace]
        for x in tmp[["thput-b"]].values.flatten():
            li.append(float(x))
    return np.array(li)

def plot_trace(trace: str):
    plt.clf()
    mpl.rcParams["figure.figsize"] = (7, 4)
    mpl.rcParams["font.size"] = 17
    # algos = ["FIFO", "LRU", "LFU", "Redis"]
    algos = ["FIFO", "LRU", "LFU"]
    # traces = ["Twitter", "MSR"]
    threads = [20, 72]
    speed_up = []
    for thread in threads:
        for algo in algos:
            print(algo, trace)
            if algo == "Redis":
                print("sampling", trace)
                data = get_data_from_csv_by_algo_trace_thread("Redis FH", trace, thread)
                fh_data = get_data_from_csv_by_algo_trace_thread("LRU FH", trace, thread)
                tmp = fh_data / data
                tmp = [x - 1 for x in tmp]
                speed_up.append(tmp)
            else:
                data = get_data_from_csv_by_algo_trace_thread(algo, trace, thread)
                fh_data = get_data_from_csv_by_algo_trace_thread(algo + " FH", trace, thread)
                tmp = fh_data / data
                tmp = [x - 1 for x in tmp]
                speed_up.append(tmp)
        tmp_haku = np.array([])
        speed_up.append(tmp_haku)
    speed_up = speed_up[0:7]
    flier_props = {
        "markersize": 8
    }
    capprops = {
        "linewidth": 1.7
    }
    boxpros = {
        "linewidth": 1.7
    }
    meanprops = {
        "markersize": 8
    }
    medianprops = {
        "linewidth": 1.7
    }
    whiskerprops = {
        "linewidth": 1.7
    }
    print(len(speed_up))
    plt.boxplot(speed_up, whis=20, showfliers=True, showmeans=True, labels=["FIFO", "LRU\n20-threads", "LFU", "", "FIFO", "LRU\n72-threads", "LFU"], flierprops=flier_props, capprops=capprops, boxprops=boxpros, meanprops=meanprops, medianprops=medianprops, whiskerprops=whiskerprops, positions=[1, 2, 3, 4, 4.5, 5.5, 6.5])
    plt.xticks([1, 2, 3, 4.5, 5.5, 6.5], ["FIFO", "LRU", "LFU", "FIFO", "LRU", "LFU"])
    if trace == "Twitter":
        plt.text(1.2, -1.8, "20-thread", fontweight="semibold")
        plt.text(4.7, -1.8, "72-thread", fontweight="semibold")
    else:
        plt.text(1.2, -1.0, "20-thread", fontweight="semibold")
        plt.text(4.7, -1.0, "72-thread", fontweight="semibold")
    plt.plot(np.arange(0.5, 7, 0.1), np.zeros(len(np.arange(0.5, 7, 0.1))), linestyle="dashed", linewidth=2)
    if trace == "MSR":
        plt.yticks(np.array([0, 0.5, 1.0, 1.5, 2, 2.5, 3]), labels=["0%", "50%", "100%", "150%", "200%", "250%", "300%"], rotation=45)
    elif trace == "Twitter":
        plt.yticks(np.array([0, 2, 4, 6, 8, 10]), labels=["0%", "200%", "400%", "600%", "800%", ""], rotation=45)
    plt.ylabel("Throughput improvement by FH")
    # plt.xticks(rotation=30)
    plt.tight_layout()
    plt.savefig("new-" + str(trace) + ".pdf", bbox_inches='tight')

trace_list = [
    "prn_0",
    "prn_1",
    "proj_1",
    "proj_2",
    "proj_4",
    "prxy_0",
    "prxy_1",
    "src1_0",
    "src1_1",
    "usr_1",
    "usr_2",
    "web_2",
    "cluster17",
    "cluster18",
    "cluster24",
    "cluster29",
    "cluster44",
    "cluster45",
    "cluster52",
]

msr_list = [
    "prn_0",
    "prn_1",
    "proj_1",
    "proj_2",
    "proj_4",
    "prxy_0",
    "prxy_1",
    "src1_0",
    "src1_1",
    "usr_1",
    "usr_2",
    "web_2",
]

twitter_list = [
    "cluster17",
    "cluster18",
    "cluster24",
    "cluster29",
    "cluster44",
    "cluster45",
    "cluster52",
]

size_list = [
    0.1,
    # 0.01,
    # 0.001
]

if __name__ == "__main__":
    plot_trace("Twitter")
    plot_trace("MSR")
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import math

def get_data_from_csv(algo_type: str):
    threads = [1, 20, 40, 60, 72]
    res = []
    for thread in threads:
        li = []
        for csv_file in csv_files:
            df = pd.read_csv(csv_file)
            temp_algo = algo_type
            df = df[df["algo type"] == temp_algo]
            df = df[df["thread"] == thread]
            for x in df[["thput-b"]].values.flatten():
                li.append(x)
        res.append(np.array(li))
    return res


def plot_trace():
    plt.clf()

    mpl.rcParams["figure.figsize"] = (6, 4)
    mpl.rcParams["font.size"] = 17
    
    speed_up = []
    algos = ["FIFO", "LRU", "LFU"]
    for algo in algos:
        print(algo)
        thput = get_data_from_csv(algo)
        thput = [x / 1000000 for x in thput]
        fh_thput = get_data_from_csv(algo + " FH")
        fh_thput = [x / 1000000 for x in fh_thput]
        for i in range(len(thput)):
            if len(speed_up) == 5:
                speed_up[i] = np.append(speed_up[i], fh_thput[i] / thput[i])
            else:    
                speed_up.append(fh_thput[i] / thput[i])
    for i in range(len(speed_up)):
        speed_up[i] = [x - 1 for x in speed_up[i] if math.isnan(x) == False]
    flier_props = {
        "markersize": 8
    }
    capprops = {
        "linewidth": 2
    }
    boxpros = {
        "linewidth": 2
    }
    meanprops = {
        "markersize": 8
    }
    medianprops = {
        "linewidth": 2
    }
    whiskerprops = {
        "linewidth": 2
    }
    print(np.max(speed_up[0]), np.max(speed_up[1]), np.max(speed_up[2]), np.max(speed_up[3]), np.max(speed_up[4]))
    plt.boxplot(speed_up, labels=["1", "20", "40", "60", "72"], whis=200, showfliers=True, showmeans=True, flierprops=flier_props, capprops=capprops, boxprops=boxpros, meanprops=meanprops, medianprops=medianprops, whiskerprops=whiskerprops)
    plt.plot(np.arange(0.5, 6.5), np.zeros(6), linestyle="dashed", linewidth=2)
    plt.yticks(np.array([0, 2, 4, 6, 8, 10]), labels=["0%", "200%", "400%", "600%", "800%", ""], rotation=45)
    plt.xlabel("Number of threads")
    plt.ylabel("Throughput improvement by FH")
    plt.tight_layout()
    
    plt.savefig("boxplot-twitter-all" + ".pdf", bbox_inches='tight')

csv_files = [
    "twitter.csv"
]

if __name__ == "__main__":
    plot_trace()
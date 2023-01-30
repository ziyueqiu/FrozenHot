import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def get_data_from_csv(csvfile: str, data_type: str, refresh_ratio):
    df = pd.read_csv(csvfile)
    df = df[df["refresh ratio"] == refresh_ratio]
    li = []
    if refresh_ratio == "no FH":
        li.append(df[(df.algo_type == "FIFO") & (df.thread == 72)][[data_type]].values.flatten()[0])
        li.append(df[(df.algo_type == "LRU") & (df.thread == 72)][[data_type]].values.flatten()[0])
        li.append(df[(df.algo_type == "LFU") & (df.thread == 72)][[data_type]].values.flatten()[0])
    else:
        li.append(df[(df.algo_type == "FIFO FH") & (df.thread == 72)][[data_type]].values.flatten()[0])
        li.append(df[(df.algo_type == "LRU FH") & (df.thread == 72)][[data_type]].values.flatten()[0])
        li.append(df[(df.algo_type == "LFU FH") & (df.thread == 72)][[data_type]].values.flatten()[0])
    return np.array(li)


if __name__ == "__main__":
    csv_file = "figure.csv"

    x = np.array([1, 2, 3])
    x_labels = np.array(["FIFO-FH", "LRU-FH", "LFU-FH"])

    mpl.rcParams["figure.figsize"] = (6.5, 4)
    mpl.rcParams["font.size"] = 17

    hhvm_lru_thput = get_data_from_csv(csv_file, "thput-b", "100") / 1000000
    redis_lru_thput = get_data_from_csv(csv_file, "thput-b", "20") / 1000000
    strict_lru_thput = get_data_from_csv(csv_file, "thput-b", "10") / 1000000
    lfu_thput = get_data_from_csv(csv_file, "thput-b", "no FH") / 1000000
    trace_1_max = max(hhvm_lru_thput[0], redis_lru_thput[0], strict_lru_thput[0], lfu_thput[0])
    trace_2_max = max(hhvm_lru_thput[1], redis_lru_thput[1], strict_lru_thput[1], lfu_thput[1])
    trace_3_max = max(hhvm_lru_thput[2], redis_lru_thput[2], strict_lru_thput[2], lfu_thput[2])

    max_thput = [trace_1_max, trace_2_max, trace_3_max]
    thput = [hhvm_lru_thput, redis_lru_thput, strict_lru_thput, lfu_thput]

    width = 0.20

    plt.bar(
        x - width * 3 / 2,
        hhvm_lru_thput,
        width,
        color='0.25',
        label=r'$100\times$',
        edgecolor="black",
    )

    plt.bar(
        x - width / 2,
        redis_lru_thput,
        width,
        color='0.45',
        label=r'$20\times$',
        edgecolor="black",
    )

    plt.bar(
        x + width / 2,
        strict_lru_thput,
        width,
        color='0.7',
        label=r'$10\times$',
        edgecolor="black",
    )

    plt.bar(
        x + width * 3 / 2,
        lfu_thput,
        width,
        color='1',
        label='No-FH',
        edgecolor="black",
    )

    plt.xlabel("Algorithms")
    plt.xticks(x, labels=x_labels, fontsize=14)
    plt.ylabel("Throughput (MQPS)")
    plt.legend(frameon=False, ncol=5, loc=(0, 1), fontsize=14, handlelength=1.5, columnspacing=1)
    plt.tight_layout()
    plt.savefig("Figure-19-rebuild-ratio-45.pdf", bbox_inches='tight')
    plt.clf()
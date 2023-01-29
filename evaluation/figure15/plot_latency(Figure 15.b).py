import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def get_data_from_csv(csvfile: str, Type: str, data_type: str):
    df = pd.read_csv(csvfile)
    df = df[df["algo_type"] == Type]
    li = []
    li.append(df[df["disk lat"] == 5][[data_type]].values.flatten()[0])
    li.append(df[df["disk lat"] == 15][[data_type]].values.flatten()[0])
    li.append(df[df["disk lat"] == 25][[data_type]].values.flatten()[0])
    li.append(df[df["disk lat"] == 35][[data_type]].values.flatten()[0])
    li.append(df[df["disk lat"] == 45][[data_type]].values.flatten()[0])
    li.append(df[df["disk lat"] == 55][[data_type]].values.flatten()[0])
    li.append(df[df["disk lat"] == 100][[data_type]].values.flatten()[0])
    # li.append(df[df["disk lat"] == 200][[data_type]].values.flatten()[0])
    li.append(df[df["disk lat"] == 500][[data_type]].values.flatten()[0])
    return np.array(li)


if __name__ == "__main__":
    csv_file = "data.csv"

    threads = np.array([5, 15, 25, 35, 45, 55, 100, 500])

    mpl.rcParams["figure.figsize"] = (6, 4)
    mpl.rcParams["font.size"] = 17

    hhvm_lru_fh_thput = get_data_from_csv(csv_file, "LRU FH", "thput-b") / 1000000
    hhvm_lru_thput = get_data_from_csv(csv_file, "LRU", "thput-b") / 1000000
    redis_lru_thput = get_data_from_csv(csv_file, "Redis LRU", "thput-b") / 1000000
    strict_lru_thput = get_data_from_csv(csv_file, "Strict LRU", "thput-b") / 1000000
    # fifo_thput = get_data_from_csv(csv_file, "FIFO", "thput-b") / 1000000

    plt.plot(
        threads,
        hhvm_lru_fh_thput,
        color='0',
        label='LRU-FH',
        linestyle='solid',
        marker="^",
        markersize=5
    )

    plt.plot(
        threads,
        redis_lru_thput,
        color='0.4',
        label='Sampling',
        linestyle='dashdot',
        marker="D",
        markersize=5
    )

    plt.plot(
        threads,
        hhvm_lru_thput,
        color='0.7',
        label='Relaxed-LRU',
        linestyle='dashed',
        marker="8",
        markersize=5
    )

    plt.plot(
        threads,
        strict_lru_thput,
        color='0.2',
        label='LRU',
        linestyle='dashed',
        marker="*",
        markersize=5
    )

    # plt.plot(
    #     threads,
    #     fifo_thput,
    #     color='0.5',
    #     label='FIFO',
    #     linestyle='dashed',
    #     marker="D",
    #     markersize=5
    # )


    # for i in range(len(threads)):
    #     plt.text(threads[i] - 0.05, hhvm_lru_fh_thput[i] + 20, s=hit_ratio[i], fontsize=15)

    plt.xlabel("Disk Latency (us)")
    plt.ylim(0, np.max(hhvm_lru_fh_thput) * 1.2)
    plt.xticks([5, 100, 200, 300, 400, 500], labels=[5, 100, 200, 300, 400, 500])
    plt.ylabel("Throughput (MQPS)")
    plt.legend(frameon=False, loc='upper right')
    plt.tight_layout()
    plt.savefig("Latency.pdf")
    plt.clf()
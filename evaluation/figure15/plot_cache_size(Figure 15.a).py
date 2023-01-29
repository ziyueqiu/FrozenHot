import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def get_data_from_csv(csvfile: str, Type: str, data_type: str):
    df = pd.read_csv(csvfile)
    df = df[df["algo_type"] == Type]
    li = []
    for x in df[[data_type]].values.flatten():
        li.append(x)
    print(li)
    return np.array(li)


if __name__ == "__main__":
    csv_file = "data.csv"

    threads = np.array([0.004, 0.25, 0.5, 0.62, 0.65])
    hit_ratio = ["50%", "90%", "97%", "99%", "100%"]
    x_labels = [0, 0.2, 0.4, 0.6, 0.8, 1.0]

    mpl.rcParams["figure.figsize"] = (6, 4)
    mpl.rcParams["font.size"] = 17

    hhvm_lru_fh_thput = get_data_from_csv(csv_file, "LRU FH", "thput-b") / 1000000
    hhvm_lru_thput = get_data_from_csv(csv_file, "LRU", "thput-b") / 1000000
    redis_lru_thput = get_data_from_csv(csv_file, "Redis LRU", "thput-b") / 1000000
    strict_lru_thput = get_data_from_csv(csv_file, "Strict LRU", "thput-b") / 1000000

    plt.plot(
        threads,
        hhvm_lru_fh_thput,
        color='0',
        label='LRU-FH',
        linestyle='solid',
        marker="^",
        markersize=10
    )

    plt.plot(
        threads,
        redis_lru_thput,
        color='0.4',
        label='Sampling',
        linestyle='dashdot',
        marker="D",
        markersize=10
    )

    plt.plot(
        threads,
        hhvm_lru_thput,
        color='0.7',
        label='Relaxed-LRU',
        linestyle='dashed',
        marker="8",
        markersize=10
    )

    plt.plot(
        threads,
        strict_lru_thput,
        color='0.2',
        label='LRU',
        linestyle='dashed',
        marker="*",
        markersize=10
    )

    bias = [0, 0, 0, -0.05, 0]
    for i in range(len(threads)):
        plt.text(threads[i] - 0.05 + bias[i], hhvm_lru_fh_thput[i] + 20, s=hit_ratio[i], fontsize=15)

    plt.xlabel("Cache Size")
    plt.ylim(0, np.max(hhvm_lru_fh_thput) * 1.2)
    plt.xlim(-0.05, 0.7)
    plt.ylabel("Throughput (MQPS)")
    plt.legend(frameon=False, loc='upper left')
    plt.tight_layout()
    plt.savefig("CacheSize.pdf")
    plt.clf()
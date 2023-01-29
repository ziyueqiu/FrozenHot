import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def get_data_from_csv(csvfile: str, data_type: str, refresh_ratio):
    df = pd.read_csv(csvfile)
    # print(df)
    # df = df[df["algo type"] == Type]
    df = df[df["refresh ratio"] == refresh_ratio]
    # df = df[df["trace"] == workload]
    # df = df[df["thread"] == thread]
    li = []
    # print(df[df.trace == "MSR"])
    # li.append(df[(df.trace == "MSR") & (df.thread == 1)][[data_type]].values.flatten()[0])
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

    # threads = np.array([1, 20, 40, 60, 80])
    # x = np.array([1.25, 2.5, 3.75, 5])
    # x = np.array([1.25, 2.5, 3.75])
    x = np.array([1, 2, 3])
    # hit_ratio = ["50%", "90%", "97%", "99%", "100%"]
    # x_labels = [1, 20, 40, 60, 80]
    x_labels = np.array(["FIFO-FH", "LRU-FH", "LFU-FH"])
    # x_labels = ["0.004%, "]
    # redis_thread = np.array([1, 20, 40, 60, 80])
    # redis_thread = np.array([1, 20, 40, 59, 60, 61, 80])

    mpl.rcParams["figure.figsize"] = (6.5, 4)
    mpl.rcParams["font.size"] = 17

    # hhvm_lru_fh_thput = get_data_from_csv(csv_file, "thput-b", "0") / 1000000
    hhvm_lru_thput = get_data_from_csv(csv_file, "thput-b", "100") / 1000000
    redis_lru_thput = get_data_from_csv(csv_file, "thput-b", "20") / 1000000
    strict_lru_thput = get_data_from_csv(csv_file, "thput-b", "10") / 1000000
    lfu_thput = get_data_from_csv(csv_file, "thput-b", "no FH") / 1000000
    # print(len(hhvm_lru_fh_thput), len(hhvm_lru_thput), len(redis_lru_thput), len(strict_lru_thput), len(lfu_thput))
    # print(hhvm_lru_fh_thput, hhvm_lru_thput, redis_lru_thput, strict_lru_thput, lfu_thput)
    trace_1_max = max(hhvm_lru_thput[0], redis_lru_thput[0], strict_lru_thput[0], lfu_thput[0])
    trace_2_max = max(hhvm_lru_thput[1], redis_lru_thput[1], strict_lru_thput[1], lfu_thput[1])
    trace_3_max = max(hhvm_lru_thput[2], redis_lru_thput[2], strict_lru_thput[2], lfu_thput[2])
    # trace_4_max = max(hhvm_lru_fh_thput[3], hhvm_lru_thput[3], redis_lru_thput[3], strict_lru_thput[3], lfu_thput[3])

    max_thput = [trace_1_max, trace_2_max, trace_3_max]
    # print(max_thput)
    # max_thput = [trace_1_max, trace_2_max, trace_3_max, trace_4_max]
    thput = [hhvm_lru_thput, redis_lru_thput, strict_lru_thput, lfu_thput]

    # for t in thput:
    #     for i in range(len(t)):
    #         t[i] = t[i] / max_thput[i]
    # plt.subplot(2, 1, 2)
    width = 0.20
    # bias = [1, 0, 2]
    # loc_bias = [0, 0, 0]
    # extra_text = ["", "", ""]
    # for s in range(len(max_thput)):
    #     plt.text(x[s] - loc_bias[s] - (3 - bias[s]) * width, 1.05, format(max_thput[s], '.1f') + extra_text[s])

    # plt.text(2.22, 1.05, "MQPS", fontsize=12)
    # print(x, x - width * 2, hhvm_lru_fh_thput)
    # plt.bar(
    #     x - width * 2,
    #     hhvm_lru_fh_thput,
    #     width,
    #     color='0',
    #     label='No-rebuild',
    #     edgecolor="black",
    #     # linestyle='solid',
    #     # marker="^",
    #     # markersize=10
    # )

    plt.bar(
        x - width * 3 / 2,
        hhvm_lru_thput,
        width,
        color='0.25',
        label=r'$100\times$',
        edgecolor="black",
        # linestyle='dashed',
        # marker="8",
        # markersize=10
    )

    plt.bar(
        x - width / 2,
        redis_lru_thput,
        width,
        color='0.45',
        label=r'$20\times$',
        edgecolor="black",
        # linestyle='dashdot',
        # marker="D",
        # markersize=10
    )

    plt.bar(
        x + width / 2,
        strict_lru_thput,
        width,
        color='0.7',
        label=r'$10\times$',
        edgecolor="black",
        # hatch="////"
        # linestyle='dashed',
        # marker="*",
        # markersize=10
    )

    plt.bar(
        x + width * 3 / 2,
        lfu_thput,
        width,
        color='1',
        label='No-FH',
        edgecolor="black",
        # linestyle='dashed',
        # marker="*",
        # markersize=10
    )

    plt.xlabel("Algorithms")
    plt.xticks(x, labels=x_labels, fontsize=14)
    plt.ylabel("Throughput (MQPS)")
    plt.legend(frameon=False, ncol=5, loc=(0, 1), fontsize=14, handlelength=1.5, columnspacing=1)
    plt.tight_layout()
    plt.savefig("Figure-19-rebuild-ratio-45.pdf", bbox_inches='tight')
    plt.clf()
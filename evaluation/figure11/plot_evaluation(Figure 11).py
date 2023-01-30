import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def cat_bar(lru_hit_lat, lru_fh_20_hit_lat, fifo_hit_lat, lfu_fh_hit_lat, fifo_fh_hit_lat, lfu_hit_lat,
            # lru_hit_ratio, lru_fh_20_hit_ratio, redis_hit_ratio, fifo_hit_ratio, lfu_fh_hit_ratio, fifo_fh_hit_ratio, lfu_hit_ratio, strict_lru_ratio,
            lru_20_fc_hit_ratio, lfu_fc_hit_ratio, fifo_fc_hit_ratio,
            tbb_fifo_fh_hit_lat, tbb_lfu_fh_hit_lat, tbb_lru_fh_hit_lat,
            tbb_fifo_fc_hit_ratio, tbb_lfu_fc_hit_ratio, tbb_lru_fc_hit_ratio,
            ):
    print(lru_hit_lat[1] / tbb_lru_fh_hit_lat[1])
    print(lfu_hit_lat[1] / tbb_lfu_fh_hit_lat[1])
    labels_right = ["FIFO", "LRU", "LFU", "FIFO", "LRU", "LFU"]
    label = ["", ""]
    width = 0.8
    x = np.arange(len(label)) * 11
    plt.figure()
    bar_1 = plt.bar(
        x - width * 5,
        fifo_hit_lat,
        width,
        label="Baseline",
        color="0",
        edgecolor="black"
    )
    bar_2 = plt.bar(
        x - width * 4,
        tbb_fifo_fh_hit_lat,
        width,
        label="FH-TBB",
        color="0.5",
        edgecolor="black"
    )
    bar_3 = plt.bar(
        x - width * 3,
        fifo_fh_hit_lat,
        width,
        label="FH",
        color="1",
        edgecolor="black"
    )
    bar_4 = plt.bar(
        x - width * 1,
        lru_hit_lat,
        width,
        color="0",
        edgecolor="black",
    )
    bar_5 = plt.bar(
        x,
        tbb_lru_fh_hit_lat,
        width,
        color="0.5",
        edgecolor="black"
    )
    bar_6 = plt.bar(
        x + width,
        lru_fh_20_hit_lat,
        width,
        color="1",
        edgecolor="black"
    )
    bar_7 = plt.bar(
        x + width * 3,
        lfu_hit_lat,
        width,
        color="0",
        edgecolor="black"
    )
    bar_8 = plt.bar(
        x + width * 4,
        tbb_lfu_fh_hit_lat,
        width,
        color="0.5",
        edgecolor="black"
    )
    bar_9 = plt.bar(
        x + width * 5,
        lfu_fh_hit_lat,
        width,
        color="1",
        edgecolor="black"
    )
    bias = 0.25
    print(x)
    x_tick_pos = [x[0] - width * 4, x[0], x[0] + width * 4, x[1] - width * 4, x[1], x[1] + width * 4]
    plt.text(x[0] + width * 4 - 0.38, tbb_lfu_fh_hit_lat[0] + 0.1, str(round(tbb_lfu_fc_hit_ratio[0] * 100)) + "%", fontsize=11)
    plt.text(x[1] + width * 4 - bias, tbb_lfu_fh_hit_lat[1] + 0.1, str(round(tbb_lfu_fc_hit_ratio[1] * 100)) + "%", fontsize=11)
    plt.text(x[0] + width * 5 - bias, lfu_fh_hit_lat[0] + 0.05, str(round(lfu_fc_hit_ratio[0] * 100)) + "%", fontsize=11)
    plt.text(x[1] + width * 5 - bias, lfu_fh_hit_lat[1] + 0.1, str(round(lfu_fc_hit_ratio[1] * 100)) + "%", fontsize=11)
    
    plt.text(x[0] - width * 4 - bias - 0.4, tbb_fifo_fh_hit_lat[0] + 0.1, str(round(tbb_fifo_fc_hit_ratio[0] * 100)) + "%", fontsize=11)
    plt.text(x[1] - width * 4 - bias, tbb_fifo_fh_hit_lat[1] + 0.1, str(round(tbb_fifo_fc_hit_ratio[1] * 100)) + "%", fontsize=11)
    plt.text(x[0] - width * 3 - bias, fifo_fh_hit_lat[0] + 0.1, str(round(fifo_fc_hit_ratio[0] * 100)) + "%", fontsize=11)
    plt.text(x[1] - width * 3 - bias, fifo_fh_hit_lat[1] + 0.1, str(round(fifo_fc_hit_ratio[1] * 100)) + "%", fontsize=11)
    
    plt.text(x[0] - bias, tbb_lru_fh_hit_lat[0] + 0.2, str(round(tbb_lru_fc_hit_ratio[0] * 100)) + "%", fontsize=11)
    plt.text(x[1] - bias, tbb_lru_fh_hit_lat[1] + 0.1, str(round(tbb_lru_fc_hit_ratio[1] * 100)) + "%", fontsize=11)
    plt.text(x[0] + width - bias, lru_fh_20_hit_lat[0] + 0.05, str(round(lru_20_fc_hit_ratio[0] * 100)) + "%", fontsize=11)
    plt.text(x[1] + width - bias, lru_fh_20_hit_lat[1] + 0.1, str(round(lru_20_fc_hit_ratio[1] * 100)) + "%", fontsize=11)
    
    plt.text(-2.0, -0.6, "20-thread", fontweight="semibold")
    plt.text(9.0, -0.6, "72-thread", fontweight="semibold")
    
    plt.legend(frameon=False, fontsize=15, loc=(0.03, 0.5))
    plt.ylabel("Average hit latency (us)", fontsize=12)
    plt.xticks(x_tick_pos, labels=labels_right)
    plt.tight_layout()
    plt.savefig("Figure16-latency-fifo-lru-lfu.pdf", bbox_inches='tight')
    plt.clf()


def get_data_from_csv(csvfile: str, thread: int, Type: str, data_type: str) -> list:
    df = pd.read_csv(csvfile)
    df = df[df["algo type"] == Type]
    li = []
    li.append(df[(df.trace == "cluster17") & (df.thread == 20)][[data_type]].values.flatten()[0])
    li.append(df[(df.trace == "cluster17") & (df.thread == 72)][[data_type]].values.flatten()[0])
    return li


if __name__ == "__main__":
    mpl.rcParams["font.size"] = 15
    mpl.rcParams["figure.figsize"] = (7, 2.8)
    cat_bar(
        np.array(get_data_from_csv("all.csv", 72, "LRU", "hit lat")) + 0.12,
        np.array(get_data_from_csv("all.csv", 72, "LRU FH", "hit lat")) + 0.12,
        # np.array(get_data_from_csv("all.csv", 72, "Redis", "hit lat")) + 0.12,
        np.array(get_data_from_csv("all.csv", 72, "FIFO", "hit lat")) + 0.12,
        np.array(get_data_from_csv("all.csv", 72, "LFU FH", "hit lat")) + 0.12,
        np.array(get_data_from_csv("all.csv", 72, "FIFO FH", "hit lat")) + 0.12,
        np.array(get_data_from_csv("all.csv", 72, "LFU", "hit lat")) + 0.12,
        # np.array(get_data_from_csv("all.csv", 72, "Strict LRU", "hit lat")) + 0.12,

        # np.array(get_data_from_csv("all.csv", 72, "LRU", "hit ratio")),
        # np.array(get_data_from_csv("all.csv", 72, "LRU FH", "hit ratio")),
        # np.array(get_data_from_csv("all.csv", 72, "Redis", "hit ratio")),
        # np.array(get_data_from_csv("all.csv", 72, "FIFO", "hit ratio")),
        # np.array(get_data_from_csv("all.csv", 72, "LFU FH", "hit ratio")),
        # np.array(get_data_from_csv("all.csv", 72, "FIFO FH", "hit ratio")),
        # np.array(get_data_from_csv("all.csv", 72, "LFU", "hit ratio")),
        # np.array(get_data_from_csv("all.csv", 72, "Strict LRU", "hit ratio")),

        np.array(get_data_from_csv("all.csv", 72, "LRU FH", "hit on fc")),
        np.array(get_data_from_csv("all.csv", 72, "LFU FH", "hit on fc")),
        np.array(get_data_from_csv("all.csv", 72, "FIFO FH", "hit on fc")),

        np.array(get_data_from_csv("all.csv", 72, "FIFO FH(LO)", "hit lat")) + 0.12,
        np.array(get_data_from_csv("all.csv", 72, "LFU FH(LO)", "hit lat")) + 0.12,
        np.array(get_data_from_csv("all.csv", 72, "LRU FH(LO)", "hit lat")) + 0.12,
        np.array(get_data_from_csv("all.csv", 72, "FIFO FH(LO)", "hit on fc")),
        np.array(get_data_from_csv("all.csv", 72, "LFU FH(LO)", "hit on fc")),
        np.array(get_data_from_csv("all.csv", 72, "LRU FH(LO)", "hit on fc")),
    )

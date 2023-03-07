import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

if __name__ == "__main__":

    mpl.rcParams["figure.figsize"] = (9.5, 4.3)
    mpl.rcParams["font.size"] = 12
    mpl.rcParams['pdf.fonttype'] = 42
    mpl.rcParams['ps.fonttype'] = 42

    ### baseline
    msr_lru_hit_ratio_win = 6 / 12
    msr_fifo_hit_ratio_win = 6 / 12
    msr_lru_thput_win = 3 / 12
    msr_fifo_thput_win = 9 / 12
    twitter_lru_hit_ratio_win = 7 / 7
    twitter_fifo_hit_ratio_win = 0 / 7
    twitter_lru_thput_win = 0 / 7
    twitter_fifo_thput_win = 7 / 7

    labels_1 = ["MSR Hit Ratio", "Twitter Hit Ratio"]
    labels_2 = ["MSR Thput", "Twitter Thput"]
    values_1_up = np.array([msr_lru_hit_ratio_win, twitter_lru_hit_ratio_win])
    values_2_up = np.array([msr_fifo_hit_ratio_win, twitter_fifo_hit_ratio_win])
    values_1_down = np.array([msr_lru_thput_win, twitter_lru_thput_win])
    values_2_down = np.array([msr_fifo_thput_win, twitter_fifo_thput_win])
    print(values_1_up, values_2_up, values_1_down, values_2_down)

    bar_height = 0.6

    plt.subplot(2, 2, 1)
    plt.barh(labels_1, values_1_up, label="LRU Wins", height=bar_height)
    plt.barh(labels_1, values_2_up, left = values_1_up, label="FIFO Wins", height=bar_height)
    plt.legend(frameon=False, loc=(0, 1), ncol=2)
    plt.xlim(0, 1)
    plt.xticks([0, 0.2, 0.4, 0.6, 0.8, 1], ["0%", "20%", "40%", "60%", "80%", "100%"])
    plt.xlabel("Percentile of Traces")
    plt.subplot(2, 2, 3)
    plt.barh(labels_2, values_1_down, label="LRU Wins", height=bar_height)
    plt.barh(labels_2, values_2_down, left = values_1_down, label="FIFO Wins", height=bar_height)
    plt.xlim(0, 1)
    plt.xticks([0, 0.2, 0.4, 0.6, 0.8, 1], ["0%", "20%", "40%", "60%", "80%", "100%"])
    plt.xlabel("Percentile of Traces")
    plt.title("Baseline", y=-1.3, fontweight="semibold")

    ### best FH
    msr_lru_hit_ratio_win = 8 / 12
    msr_fifo_hit_ratio_win = 4 / 12
    msr_lru_thput_win = 9 / 12
    msr_fifo_thput_win = 3 / 12
    twitter_lru_hit_ratio_win = 6 / 7
    twitter_fifo_hit_ratio_win = 1 / 7
    twitter_lru_thput_win = 4 / 7
    twitter_fifo_thput_win = 3 / 7

    labels_1 = ["", " "]
    labels_2 = ["", " "]
    values_1_up = np.array([msr_lru_hit_ratio_win, twitter_lru_hit_ratio_win])
    values_2_up = np.array([msr_fifo_hit_ratio_win, twitter_fifo_hit_ratio_win])
    values_1_down = np.array([msr_lru_thput_win, twitter_lru_thput_win])
    values_2_down = np.array([msr_fifo_thput_win, twitter_fifo_thput_win])

    plt.subplot(2, 2, 2)
    plt.barh(labels_1, values_1_up, label="LRU Wins", height=bar_height)
    plt.barh(labels_1, values_2_up, left = values_1_up, label="FIFO Wins", height=bar_height)
    # plt.legend(frameon=False, loc=(0, 1), ncol=2)
    plt.xlim(0, 1)
    plt.xticks([0, 0.2, 0.4, 0.6, 0.8, 1], ["0%", "20%", "40%", "60%", "80%", "100%"])
    plt.xlabel("Percentile of Traces")
    plt.subplot(2, 2, 4)
    plt.barh(labels_2, values_1_down, label="LRU Wins", height=bar_height)
    plt.barh(labels_2, values_2_down, left = values_1_down, label="FIFO Wins", height=bar_height)
    plt.xlim(0, 1)
    plt.xlabel("Percentile of Traces")
    plt.xticks([0, 0.2, 0.4, 0.6, 0.8, 1], ["0%", "20%", "40%", "60%", "80%", "100%"])
    plt.title("Best FH", y=-1.3, fontweight="semibold")

    plt.tight_layout()
    plt.subplots_adjust(hspace=1.1, wspace=0.15)
    plt.savefig("Figure-winner-all.pdf", bbox_inches='tight')
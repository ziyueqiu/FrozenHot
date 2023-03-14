import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def get_data_from_csv(csvfile: str, baseline, trace_type: str):
    df = pd.read_csv(csvfile)
    if baseline:
        df = df[df["refresh ratio"] == "no FH"]
        df = df[(df.algo_type == "FIFO") | (df.algo_type == "LRU")]
    else:
        df = df[df["refresh ratio"] != "no FH"]
        df = df[(df.algo_type == "FIFO FH") | (df.algo_type == "LRU FH")]
    if trace_type == "Twitter":
        df = df[df['trace'].str.contains("Twitter")]
    else:
        df = df[df['trace'].str.contains("MSR")]
    if baseline:
        tmp_df = df[(df.algo_type == "FIFO") & (df.thread == 72)][["trace", "thput-b"]].groupby(by="trace", as_index=False).max()
    else:
        tmp_df = df[(df.algo_type == "FIFO FH") & (df.thread == 72)][["trace", "thput-b"]].groupby(by="trace", as_index=False).max()
    tmp_df = pd.merge(tmp_df, df, on=["trace", "thput-b"], how='left')
    fifo_thput = tmp_df[["thput-b"]].values.flatten()
    fifo_ratio = tmp_df[["total miss ratio"]].values.flatten()
    if baseline:
        tmp_df = df[(df.algo_type == "LRU") & (df.thread == 72)][["trace", "thput-b"]].groupby(by="trace", as_index=False).max()
    else:
        tmp_df = df[(df.algo_type == "LRU FH") & (df.thread == 72)][["trace", "thput-b"]].groupby(by="trace", as_index=False).max()
    tmp_df = pd.merge(tmp_df, df, on=["trace", "thput-b"], how='left')
    lru_thput = tmp_df[["thput-b"]].values.flatten()
    lru_ratio = tmp_df[["total miss ratio"]].values.flatten()
    return np.sum((lru_thput - fifo_thput) > 0), np.sum((lru_thput - fifo_thput) < 0), np.sum((lru_ratio - fifo_ratio) < 0), np.sum((lru_ratio - fifo_ratio) > 0)

if __name__ == "__main__":

    mpl.rcParams["figure.figsize"] = (9.5, 4.3)
    mpl.rcParams["font.size"] = 12
    mpl.rcParams['pdf.fonttype'] = 42
    mpl.rcParams['ps.fonttype'] = 42

    csv_file="figure.csv"
    t_b_lru_thput_win, t_b_fifo_thput_win, t_b_lru_ratio_win, t_b_fifo_ratio_win = get_data_from_csv(csv_file, True, "Twitter")
    t_f_lru_thput_win, t_f_fifo_thput_win, t_f_lru_ratio_win, t_f_fifo_ratio_win = get_data_from_csv(csv_file, False, "Twitter")
    m_b_lru_thput_win, m_b_fifo_thput_win, m_b_lru_ratio_win, m_b_fifo_ratio_win = get_data_from_csv(csv_file, True, "MSR")
    m_f_lru_thput_win, m_f_fifo_thput_win, m_f_lru_ratio_win, m_f_fifo_ratio_win = get_data_from_csv(csv_file, False, "MSR")

    ### baseline
    msr_lru_hit_ratio_win = m_b_lru_ratio_win / (m_b_lru_ratio_win + m_b_fifo_ratio_win)
    msr_fifo_hit_ratio_win = m_b_fifo_ratio_win / (m_b_lru_ratio_win + m_b_fifo_ratio_win)
    msr_lru_thput_win = m_b_lru_thput_win / (m_b_lru_thput_win + m_b_fifo_thput_win)
    msr_fifo_thput_win = m_b_fifo_thput_win / (m_b_lru_thput_win + m_b_fifo_thput_win)
    twitter_lru_hit_ratio_win = t_b_lru_ratio_win / (t_b_lru_ratio_win + t_b_fifo_ratio_win)
    twitter_fifo_hit_ratio_win = t_b_fifo_ratio_win / (t_b_lru_ratio_win + t_b_fifo_ratio_win)
    twitter_lru_thput_win = t_b_lru_thput_win / (t_b_lru_thput_win + t_b_fifo_thput_win)
    twitter_fifo_thput_win = t_b_fifo_thput_win / (t_b_lru_thput_win + t_b_fifo_thput_win)

    labels_1 = ["MSR Hit Ratio", "Twitter Hit Ratio"]
    labels_2 = ["MSR Thput", "Twitter Thput"]
    values_1_up = np.array([msr_lru_hit_ratio_win, twitter_lru_hit_ratio_win])
    values_2_up = np.array([msr_fifo_hit_ratio_win, twitter_fifo_hit_ratio_win])
    values_1_down = np.array([msr_lru_thput_win, twitter_lru_thput_win])
    values_2_down = np.array([msr_fifo_thput_win, twitter_fifo_thput_win])

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
    msr_lru_hit_ratio_win = m_f_lru_ratio_win / (m_f_lru_ratio_win + m_f_fifo_ratio_win)
    msr_fifo_hit_ratio_win = m_f_fifo_ratio_win / (m_f_lru_ratio_win + m_f_fifo_ratio_win)
    msr_lru_thput_win = m_f_lru_thput_win / (m_f_lru_thput_win + m_f_fifo_thput_win)
    msr_fifo_thput_win = m_f_fifo_thput_win / (m_f_lru_thput_win + m_f_fifo_thput_win)
    twitter_lru_hit_ratio_win = t_f_lru_ratio_win / (t_f_lru_ratio_win + t_f_fifo_ratio_win)
    twitter_fifo_hit_ratio_win = t_f_fifo_ratio_win / (t_f_lru_ratio_win + t_f_fifo_ratio_win)
    twitter_lru_thput_win = t_f_lru_thput_win / (t_f_lru_thput_win + t_f_fifo_thput_win)
    twitter_fifo_thput_win = t_f_fifo_thput_win / (t_f_lru_thput_win + t_f_fifo_thput_win)

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
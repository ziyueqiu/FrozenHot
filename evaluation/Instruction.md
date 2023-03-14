This folder contains plot scripts for EuroSys 2023 paper "FrozenHot Cache: Rethinking Cache Management for Modern Software" figure 8~15, but does not contains experiment data for processing.

## Setup

Some python lib are used to generate the final figures

```bash
pip install matplotlib
pip install numpy
pip install pandas
```

## General instruction

`run_**.py` is the script to run the trace for each figure, and requires two args and one optional arg. Take `run_figure8.py` for example.

```
usage: run_figure8.py [-h] [-o OUTPUT] Twitter_prefix MSR_prefix

Running trace script

positional arguments:
  Twitter_prefix        The data path of Twitter trace data set
  MSR_prefix            The data path of MSR trace data set

optional arguments:
  -h, --help            show this help message and exit
  -o OUTPUT, --output OUTPUT
                        Output data path
```

Data are required to see in each `origin_data/figureX/` folder. We use figure12 as an example. By default, log files will be generated in the proper location.

Every data processing script is named like "handle_data.py", while every plot script is marked with the corresponding figure name.

Scripts are classified into folders named with corresponding figure.

Figure 10, 14 are two folders without "data_handle.py", cause data used in this script is collected manually.

An example of generating figure8 in our paper is like

```bash
cd FrozenHot/evaluation/figure08
# Run experiment to generate the log files
python3 run_figure8.py /path/to/Twitter/trace/ /path/to/MSR/trace/
# Handle the log files into human-readable csvfile
python3 'handle_datafile msr.py'
python3 'handle_datafile twitter.py'
# Generate the figures used in the paper
python3 'PlotEvaluation(Figure 8.a)-twitter-boxplot.py'
python3 'PlotEvaluation(Figure 8.b)-msr-boxplot.py'
python3 'PlotEvaluation(Figure 8.c&d)-boxplot.py'
```

## Step by step AE instructions

### E1

**step1:**

First run the script to generate the original log files.

```bash
cd FrozenHot/evaluation/figure08
python3 run_figure8.py /path/to/Twitter/trace/ /path/to/MSR/trace/
```

This will generate log files in `FrozenHot/evaluation/origin_data/figure8` by default.

**step2:**

Secondly, handle those origin log files into a `.csv` file.

```bash
python3 'handle_datafile msr.py'
python3 'handle_datafile twitter.py'
```

Two csv files named `twitter.csv` and `msr.csv` will appear in the current path.

**step3:**

Lastly, use these two csv files to generate the final figure.

```bash
python3 'PlotEvaluation(Figure 8.a)-twitter-boxplot.py'
python3 'PlotEvaluation(Figure 8.b)-msr-boxplot.py'
python3 'PlotEvaluation(Figure 8.c&d)-boxplot.py'
```

### E2

First generate the original log files.

```bash
cd FrozenHot/evaluation/figure09
python3 run_figure9.py /path/to/Twitter/trace/ /path/to/MSR/trace/
```

Then move all those generated log files into `FrozenHot/evaluation/origin_data/figure9` and generate csv file.

```bash
python3 handle_datafile.py
python3 'PlotEvaluation(Figure 9)-boxplot-hit-on-fc.py'
```

Figure9 will be generated.

### E3

**step1:**

 To generate data needed for E3, we need 12 MSR traces, and 7 Twitter traces to run under  four different lifetimes (No FH,  10x, 20x, 100x, No FH is the baseline) with 72 threads concurrency. We can use `FrozenHot/evaluation/run_figure14.py`.

First generate the original log files.

```bash
cd FrozenHot/evaluation/figure14
python3 run_figure14.py /path/to/Twitter/trace/ /path/to/MSR/trace/
```

**step2**

Then use `FrozenHot/evaluation/figure14/handle_datafile.py` to handle the datafiles.

```bash
cd FrozenHot/evaluation/figure14
python3 handle_datafile.py
```

After the above operation, a csv file named `figure.csv` should be generated in the same folder.

**step3**

Run script to generate the figure.

```bash
python3 `plot_winner subplot(Figure14).py`.
```
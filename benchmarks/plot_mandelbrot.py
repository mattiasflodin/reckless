#!/usr/bin/python2
from __future__ import print_function
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
from sys import argv
import os.path

matplotlib.rc('font', size=10)

# color palette from colorbrewer2.org
COLORS = [
    '#8dd3c7',
    '#fb8072',
    '#80b1d3',
    '#ffffb3',
    '#bebada',
    '#fdb462',
]

filename = None
if len(argv) > 1:
    filename = argv[1]
    width = 858
    height = 858

fig, ax = plt.subplots()

ONLY_OVERHEAD = 1
#LOG_LINES = 1048576
MAX_CORES = 4
LIBS = ['nop', 'reckless', 'spdlog', 'stdio', 'fstream', 'pantheios']
LIBS = ['nop', 'reckless', 'stdio']
if ONLY_OVERHEAD:
    del COLORS[0]
    del LIBS[0]
ind = np.arange(MAX_CORES)
WIDTH=1.0/(len(LIBS)+2)
GROUP_WIDTH = WIDTH*len(LIBS)
GROUP_OFFSET = (1.0 - GROUP_WIDTH)/2

def read_timing(lib, cores, offset=0.0):
    with open(os.path.join('results', 'mandelbrot-%s-%d.txt' % (lib, cores)), 'r') as f:
        data = f.readlines()
    data = sorted([float(x)/1000.0+offset for x in data])
    # Use interquartile range as measure of scale.
    low, high = np.percentile(data, [25, 75])

    # Samples generally center around a minimum point that represents the ideal
    # running time. There are no outliers below the ideal time; execution does
    # not accidentally take some kind of short cut. At least, not for this
    # particular benchmark. It could be conceivable for other tests where we
    # are I/O-bound and we get lucky with the cache, but this test is not
    # I/O-bound.
    #
    # However, outliers do exist on the other end of the spectrum. Something
    # completely unrelated sometimes happens in the system and takes tons of
    # CPU cycles from our benchmark. We don't want to include these because
    # they make it more difficult to reproduce the same statistics on multiple
    # runs. Or, in statistics speak, they make our benchmark less robust and
    # less efficient. To counter this we get rid of the worst outliers on the
    # upper end, but we leave the lower end intact.
    outlier_index = int(len(data)*0.8)
    data = data[:outlier_index]
    mean = np.mean(data)
    return low, mean, high

offsets = []
for cores in range(1, MAX_CORES+1):
    if ONLY_OVERHEAD:
        _, mean, _ = read_timing('nop', cores)
        offsets.append(-mean)
    else:
        offsets.append(0.0)

rects = []
for index, lib in enumerate(LIBS):
    means = []
    error = [[], []]

    for cores in range(1, MAX_CORES+1):
        low, mean, high = read_timing(lib, cores, offsets[cores-1])
    print(lib, cores, mean, low, high)
        means.append(mean)
        error[0].append(mean - low)
        error[1].append(high - mean)
    if not ONLY_OVERHEAD:
        # IQR is so small that it just clutters the plot in this view.
        error = None
    rects.append(ax.bar(ind+index*WIDTH + GROUP_OFFSET, means, WIDTH,
        color=COLORS[index], yerr=error, ecolor='black'))

ax.legend( rects, LIBS )

if ONLY_OVERHEAD:
    ax.set_ylabel('Seconds overhead')
else:
    ax.set_ylabel('Total seconds')
ax.set_xlabel('Number of worker threads')
ax.set_xticks(ind + GROUP_OFFSET + GROUP_WIDTH/2)
ax.set_xticklabels([str(x) for x in range(1, MAX_CORES+1)])
#ax.set_title('1024x1024 mandelbrot set render, one log line per pixel (i.e. 1.05 million log lines)')

if filename is None:
    plt.show()
else:
    dpi = 96
    fig.set_size_inches(width/dpi, height/dpi)
    plt.savefig(filename, dpi=dpi)

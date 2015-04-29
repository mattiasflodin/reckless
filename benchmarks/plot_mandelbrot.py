#!/usr/bin/python2
from __future__ import print_function
import matplotlib.pyplot as plt
import numpy as np
import sys
import os.path

# color palette from colorbrewer2.org
COLORS = [
    '#fdb462',
    '#fb8072',
    '#8dd3c7',
    '#ffffb3',
    '#bebada',
    '#80b1d3',
]

fig, ax = plt.subplots()

ONLY_OVERHEAD = 1
#LOG_LINES = 1048576
MAX_CORES = 4
#LIBS = ['nop', 'reckless', 'stdio', 'fstream', 'pantheios', 'spdlog']
LIBS = ['nop', 'reckless', 'stdio', 'fstream']
if ONLY_OVERHEAD:
    del COLORS[0]
    del LIBS[0]
ind = np.arange(MAX_CORES)
WIDTH=1.0/(len(LIBS)+2)
GROUP_WIDTH = WIDTH*len(LIBS)
GROUP_OFFSET = (1.0 - GROUP_WIDTH)/2

def read_timing(lib, cores):
    with open(os.path.join('results', '%s_mandelbrot_%d.txt' % (lib, cores)), 'r') as f:
        data = f.readlines()
    data = np.array([float(x)/1000.0 for x in data])
    #return np.min(data), np.std(data)
    return np.mean(data), np.std(data)

nop_timings = []
for cores in range(1, MAX_CORES+1):
    nop_timings.append(read_timing('nop', cores))

rects = []
for index, lib in enumerate(LIBS):
    mean_timings = []
    std_timings = []
    
    for cores in range(1, MAX_CORES+1):
        mean, std = read_timing(lib, cores) 
        print(lib, cores, mean, std)
        if ONLY_OVERHEAD:
            mean -= nop_timings[cores-1][0]
        mean_timings.append(mean)
        std_timings.append(std)
    if not ONLY_OVERHEAD:
        # Standard deviation is so small that it just clutters the plot in this
        # view
        std_timings = None
    rects.append(ax.bar(ind+index*WIDTH + GROUP_OFFSET, mean_timings, WIDTH, color=COLORS[index],
        yerr=std_timings, ecolor='black'))

ax.legend( rects, LIBS )

if ONLY_OVERHEAD:
    ax.set_ylabel('Seconds overhead')
else:
    ax.set_ylabel('Total seconds')
ax.set_xlabel('Number of worker threads')
ax.set_xticks(ind + GROUP_OFFSET + GROUP_WIDTH/2)
ax.set_xticklabels([str(x) for x in range(1, MAX_CORES+1)])
ax.set_title('1024x1024 mandelbrot set render, one log line per pixel (i.e. 1.05 million log lines)')

plt.show()

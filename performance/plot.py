#!/usr/bin/python2
import matplotlib.pyplot as plt
import sys
import os.path

average_window = 6

# from http://www.mulinblog.com/a-color-palette-optimized-for-data-visualization/
gray = '#4D4D4D'
blue = '#5DA5DA'
orange = '#FAA43A'
green = '#60BD68'
pink = '#F17CB0'
brown = '#B2912F'
purple = '#B276B2'
yellow = '#DECF3F'
red = '#F15854'

def timing_kind(name):
    name = os.path.splitext(name)[0]
    return name[name.rfind('_')+1:]

def pretty_name(name):
    name_table = {
            'nop': 'Timing overhead (~113 ticks)',
            'stdio': 'fprintf (C)',
            'fstream': 'std::fstream (C++)',
            'alog': 'asynclog'
    }
            
    return name_table.get(timing_kind(name), name)

def timing_color(name):
    color_table = {
            'nop': red,
            'stdio': orange,
            'fstream': green,
            'alog': blue,
            }
    return color_table.get(timing_kind(name))

    
def average(average_window, data):
    window = []
    newdata = []
    for v in data:
        window.append(v)
        if len(window) == average_window:
            newdata.append(sum(window)/average_window)
            del window[0]
    return newdata

def average2(average_window, data):
    window = []
    newdata = []
    for v in data:
        window.append(v)
        if len(window) == average_window:
            newdata.append(sum(window)/average_window)
            del window[:]
    return newdata

fig, ax = plt.subplots()
for name in sys.argv[1:]:
    with open(name, 'r') as f:
        data = f.readlines()
    data = [int(x) for x in data]
    if average_window != 1:
        data = average2(average_window, data);
    ax.plot(data, '-', label=pretty_name(name), color=timing_color(name))

legend = ax.legend()
plt.xlabel('Iteration')
plt.ylabel('Latency (CPU ticks)')
plt.show()
#ax.set_size_inches(10, 10)
#plt.savefig('plot.png', dpi=150)

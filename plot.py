#!/usr/bin/python2
import matplotlib.pyplot as plt
import sys

average_window = 1

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
    ax.plot(data, '-', label=name)

legend = ax.legend()
plt.show()

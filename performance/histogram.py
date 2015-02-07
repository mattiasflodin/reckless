#!/usr/bin/python2
from __future__ import print_function
import sys
import matplotlib.pyplot as plt

for path in sys.argv[1:]:
    cycles = []
    with open(path, 'r') as f:
        for line in f:
            cycles.append(int(line))
            
    cycles.sort()
    cycles = cycles[100:-100]
    print(len(cycles))
    plt.hist(cycles, 1000, histtype='step')

plt.xlabel('CPU cycles')
plt.ylabel('Count')
plt.show()


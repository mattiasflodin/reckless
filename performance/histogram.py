#!/usr/bin/python2
from __future__ import print_function
import sys
import os.path
import matplotlib.pyplot as plt

#fig, ax = plt.subplots()

plots = []
for path in sys.argv[1:]:
    cycles = []
    with open(path, 'r') as f:
        for line in f:
            cycles.append(int(line))
            
    cycles.sort()
    cycles = cycles[10:-10]
    print(len(cycles))
    name = os.path.splitext(path)[0]
    name = name[name.rfind('_')+1:]
    
    plt.hist(cycles, 100, label=name, histtype='step')
    

plt.xlabel('CPU ticks')
plt.ylabel('Count')
plt.legend()
plt.show()


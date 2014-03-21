#!/usr/bin/python2
import matplotlib.pyplot as plt
import sys

fig, ax = plt.subplots()
for name in sys.argv[1:]:
    with open(name, 'r') as f:
        data = f.readlines()
    data = [int(x) for x in data]
    ax.plot(data, '.', label=name)

legend = ax.legend()
plt.show()

#!/usr/bin/python2
from pylab import *
with open("timings.txt", 'r') as f:
    data = f.readlines()
data = [int(x) for x in data[:1000]]
plot(data)

show()

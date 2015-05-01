#!/usr/bin/python2
import matplotlib.pyplot as plt
import sys
from sys import argv
from getopt import gnu_getopt
import os.path

average_window = 128

ALL_LIBS = ['nop', 'reckless', 'stdio', 'fstream', 'pantheios', 'spdlog']
ALL_TESTS = ['periodic_calls', 'call_burst', 'write_files', 'mandelbrot']

def timing_library(name):
    name = os.path.splitext(name)[0]
    return name[:name.find('_')]

def pretty_name(name):
    name_table = {
            'nop': 'Timing overhead (~113 ticks)',
            'stdio': 'fprintf (C)',
            'fstream': 'std::fstream (C++)',
            'reckless': 'reckless'
    }
            
    return name_table.get(timing_library(name), timing_library(name))

def timing_color(name):
    color_table = {
            'nop': red,
            'stdio': orange,
            'fstream': green,
            'pantheios': gray,
            'spdlog': pink,
            'reckless': blue,
            }
    return color_table[timing_library(name)]

    
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

def parse_ranges(s):
    ranges = s.split(',')
    result = []
    for r in ranges:
        parts = r.split('-')
        if len(parts)>2:
            raise ValueError("Invalid range specification: " + r)
        start=int(parts[0])
        if len(parts) == 1:
            end=int(parts[1])
        else:
            end = start
        start, end = min(start, end), max(start, end)
        result.extend(list(range(start, end+1)))
    return result
        
def main():
    opts, args = gnu_getopt(argv[1:], 'l:t:c:h', ['libs', 'tests', 'threads', 'help'])
    libs = None
    tests = None
    show_help = len(args) != 0

    for option, value in opts:
        if option in ('-l', '--libs'):
            libs = [lib.strip() for lib in value.split(',')]
        elif option in ('-t', '--tests'):
            tests = [test.strip() for test in value.split(',')]
        elif option in ('-c', '--threads'):
            threads = parse_ranges(value)
        elif option in ('-h', '--help'):
            show_help = True

    if show_help:
        stderr.write(
            'usage: plot.py [OPTIONS]\n'
            'where OPTIONS are:\n'
            '-t,--tests TESTS comma-separated list of tests to run\n'
            '-l,--libs  LIBS  comma-separated list of libs to benchmark\n'
            '-h,--help        show this help\n'
            'Available libraries: {}\n'
            'Available tests: {}\n'.format(
                ','.join(ALL_LIBS), ','.join(ALL_TESTS)))
        return 1
    
    if libs is None:
        libs = sorted(ALL_LIBS)
    if tests is None:
        tests = sorted(ALL_TESTS)

    plot(libs, tests)
    return 0

def plot(libs, tests):
    fig, ax = plt.subplots()
    for test in tests:
        for lib in libs:
            for threads in 
            name = "results/%s_%s_%d
            with open(name, 'r') as f:
                data = f.readlines()
            name = os.path.split(name)[-1]
            data = [int(x) for x in data]
            if average_window != 1:
                data = average2(average_window, data);
        ax.plot(data, '-', label=name) #, label=pretty_name(name) , color=timing_color(name))

    legend = ax.legend()
    plt.xlabel('Iteration')
    plt.ylabel('Latency (CPU ticks)')
    plt.show()
    #ax.set_size_inches(10, 10)
    #plt.savefig('plot.png', dpi=150)

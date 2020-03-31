#!/usr/bin/python2
import sys
from sys import argv, stderr
from getopt import gnu_getopt
import os.path
from math import pi, sqrt, exp

ALL_LIBS = ['nop', 'reckless', 'stdio', 'fstream', 'boost_log', 'spdlog', 'g3log']
ALL_TESTS = ['periodic_calls', 'call_burst', 'write_files'] #, 'mandelbrot']

THREADED_TESTS = {'call_burst', 'mandelbrot'}

# color palette from colorbrewer2.org
COLORS = [
    '#1b9e77',
    '#d95f02',
    '#7570b3',
    '#e7298a',
    '#66a61e',
    '#e6ab02',
    '#a6761d',
]

def get_rdtsc_frequency():
    with open('/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq', 'r') as f:
        return int(f.read())*1000

RDTSC_FREQUENCY = get_rdtsc_frequency()
TIME_SCALE = 1000000000.0/RDTSC_FREQUENCY

def get_default_window(test):
    table = {
        'periodic_calls': 8,
        'call_burst': 128,
        'write_files': 128,
        'mandelbrot': 1
    }

    return table.get(test, 1)

def pretty_name(name):
    name_table = {
            'nop': 'no operation',
            'stdio': 'fprintf (C)',
            'fstream': 'std::fstream (C++)',
            'reckless': 'reckless',
            'periodic_calls': 'periodic calls',
            'call_burst': 'single call burst',
            'write_files': 'heavy disk I/O',
            'mandelbrot': 'mandelbrot render'
    }

    return name_table.get(name, name)

def lib_color(name):
    color_table = {
            'nop': COLORS[0],
            'reckless': COLORS[3],
            'spdlog': COLORS[2],
            'stdio': COLORS[1],
            'fstream': COLORS[4],
            'boost_log': COLORS[5],
            'g3log': COLORS[6]
            }
    return color_table[name]


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
            newdata.extend([sum(window)/average_window]*average_window)
            del window[:]
    #if len(window) != 0:
    #    window.extend([0]*(average_window-len(window)))
    #    newdata.append(sum(window)/average_window)
    return newdata

def gaussian_kernel(width):
    kernel = []
    std = float(width)/6
    std = std*std
    C = 1.0/sqrt(2*pi*std)
    for x in range(0, width):
        xd = x - float(width)/2
        kernel.append(C*exp(-xd*xd/(2*std)))

    n = sum(kernel)
    kernel = [x/n for x in kernel]

    return kernel

def gaussian_average(average_window, data):
    kernel = gaussian_kernel(average_window)
    window = []
    newdata = []
    for v in data:
        window.append(v)
        if len(window) == average_window:
            newdata.append(sum([x*k for x, k in zip(window, kernel)]))
            del window[0]
    return newdata

def parse_ranges(s):
    ranges = s.split(',')
    result = []
    for r in ranges:
        parts = r.split('-')
        if len(parts)>2:
            raise ValueError("Invalid range specification: " + r)
        start=int(parts[0])
        if len(parts) == 2:
            end=int(parts[1])
        else:
            end = start
        start, end = min(start, end), max(start, end)
        result.extend(list(range(start, end+1)))
    return result

def main():
    opts, args = gnu_getopt(argv[1:], 'l:t:c:w:h', ['libs=', 'tests=',
        'threads=', 'window=', 'file=', 'top=', 'bottom=', 'iterations=',
        'title=', 'help'])
    libs = None
    tests = None
    threads = None
    window = None
    filename = None
    width = 858
    height = 858
    show_help = len(args) != 0
    top = None
    bottom = None
    iterations = None
    title = None

    for option, value in opts:
        if option in ('-l', '--libs'):
            libs = [lib.strip() for lib in value.split(',')]
        elif option in ('-t', '--tests'):
            tests = [test.strip() for test in value.split(',')]
        elif option in ('-c', '--threads'):
            threads = parse_ranges(value)
        elif option in ('-h', '--help'):
            show_help = True
        elif option in ('-w', '--window'):
            window = int(value)
        elif option == '--file':
            filename = value
        elif option == '--bottom':
            bottom = int(value)
        elif option == '--top':
            top = int(value)
        elif option == '--iterations':
            iterations = int(value)
        elif option == '--title':
            title = value

    if show_help:
        stderr.write(
            'usage: plot.py [OPTIONS]\n'
            'where OPTIONS are:\n'
            '-t,--tests    TESTS      comma-separated list of tests to plot\n'
            '-l,--libs     LIBS       comma-separated list of libs to plot\n'
            '-c,--threads  THREADS    thread-counts to include in a comma-separated list (e.g. 1-2,4)\n'
            '-w,--window   SIZE       Size of moving-average window\n'
            '--top         TOP        Top y coordinate for chart\n'
            '--bottom      BOTTOM     Bottom y coordinate for chart\n'
            '--iterations  ITERATIONS Number of iterations to include\n'
            '--title    TITLE      Plot title\n'
            '-h,--help        show this help\n'
            'Available libraries: {}\n'
            'Available tests: {}\n'.format(
                ','.join(ALL_LIBS), ','.join(ALL_TESTS)))
        return 1

    if libs is None:
        libs = sorted(ALL_LIBS)
    if tests is None:
        tests = sorted(ALL_TESTS)
    if threads is None:
        threads = list(range(1, 5))

    plot(libs, tests, threads, window, top, bottom, iterations, filename, width, height, title)
    return 0

def plot(libs, tests, threads_list, window, top, bottom, iterations, plot_filename, width, height, title, dpi=96):
    import matplotlib
    matplotlib.rc('font', size=10)
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots()

    def single_plot(filename, test, name, window, color=None):
        with open(filename, 'r') as f:
            lines = f.readlines()
        data = []
        for line in lines:
            assert line.endswith('\n')
            start, end = line[:-1].split(' ')
            start, end = int(start), int(end)
            data.append((end - start)*TIME_SCALE)
        if window is None:
            window = 1 #get_default_window(test)
        if window != 1:
            data = gaussian_average(window, data)
        if iterations is not None and iterations<len(data):
            data = data[:iterations]
        ax.plot(data, '-', label=name, color=color, linewidth=1)

    for test in tests:
        for lib in libs:
            color = lib_color(lib)
            base_name = []
            if len(libs)>1:
                base_name.append(pretty_name(lib))
            if len(tests)>1:
                base_name.append(pretty_name(test))
            if test in THREADED_TESTS:
                for threads in threads_list:
                    name = base_name[:]
                    filename = "results/%s-%s-%d.txt" % (test, lib, threads)
                    if len(threads_list)>1:
                        name.append("%d threads" % threads)
                    single_plot(filename, test, ', '.join(name), window, color)
            else:
                filename = "results/%s-%s.txt" % (test, lib)
                single_plot(filename, test, ', '.join(base_name), window, color)

    if top is not None:
        ax.set_ylim(ymax=top)
    if bottom is not None:
        ax.set_ylim(ymin=bottom)

    legend = ax.legend()
    # set the linewidth of each legend object
    for legobj in legend.legendHandles:
        legobj.set_linewidth(4)

    plt.xlabel('Iteration')
    plt.ylabel('Latency (nanoseconds)')
    if title is not None:
        fig.canvas.set_window_title(title)
    if plot_filename is None:
        plt.show()
    else:
        fig.set_size_inches(width/dpi, height/dpi)
        plt.savefig(plot_filename, dpi=dpi)

if __name__ == "__main__":
    sys.exit(main())

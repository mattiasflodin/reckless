#!/usr/bin/python2
import sys
from sys import argv, stderr
from getopt import gnu_getopt
import os.path

ALL_LIBS = ['nop', 'reckless', 'stdio', 'fstream', 'pantheios', 'spdlog']
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
]

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
            'pantheios': COLORS[5],
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
            newdata.append(sum(window)/average_window)
            del window[:]
    #if len(window) != 0:
    #    window.extend([0]*(average_window-len(window)))
    #    newdata.append(sum(window)/average_window)
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
    opts, args = gnu_getopt(argv[1:], 'l:t:c:w:h', ['libs=', 'tests=', 'threads=', 'file=', 'help'])
    libs = None
    tests = None
    threads = None
    window = None
    filename = None
    width = 1024
    height = 1024
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
        elif option in ('-w', '--window'):
            window = int(value)
        elif option == '--file':
            filename = value
            

    if show_help:
        stderr.write(
            'usage: plot.py [OPTIONS]\n'
            'where OPTIONS are:\n'
            '-t,--tests    TESTS comma-separated list of tests to plot\n'
            '-l,--libs     LIBS  comma-separated list of libs to plot\n'
            '-c,--threads  LIBS  thread-counts to include in a comma-separated list (e.g. 1-2,4)\n'
            '-w,--window   SIZE  Size of moving-average window\n'
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

    plot(libs, tests, threads, window, filename, width, height)
    return 0

def plot(libs, tests, threads_list, window, plot_filename=None, width=1024, height=1024, dpi=96):
    import matplotlib
    matplotlib.rc('font', size=10)
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots()
    
    def single_plot(filename, test, name, window, color=None):
        print(filename)
        with open(filename, 'r') as f:
            data = f.readlines()
        data = [int(x) for x in data]
        if window is None:
            window = get_default_window(test)
        if window != 1:
            data = average2(window, data);
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
                    filename = "results/%s_%s_%d.txt" % (lib, test, threads)
                    if len(threads_list)>1:
                        name.append("%d threads" % threads)
                    single_plot(filename, test, ', '.join(name), window, color)
            else:
                filename = "results/%s_%s.txt" % (lib, test)
                single_plot(filename, test, ', '.join(base_name), window, color)
    
    legend = ax.legend()
    plt.xlabel('Iteration')
    plt.ylabel('Latency (CPU ticks)')
    if plot_filename is None:
        plt.show()
    else:
        fig.set_size_inches(width/dpi, height/dpi)
        plt.savefig(plot_filename, dpi=dpi)

if __name__ == "__main__":
    sys.exit(main())

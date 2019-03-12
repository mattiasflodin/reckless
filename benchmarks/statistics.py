#!/usr/bin/python3

import sys
from sys import argv, stderr, stdout
from getopt import gnu_getopt
import numpy as np

ALL_LIBS = ['nop', 'reckless', 'stdio', 'fstream', 'boost_log', 'spdlog', 'g3log']
ALL_TESTS = ['periodic_calls', 'call_burst', 'write_files', 'mandelbrot']
THREADED_TESTS = {'call_burst', 'mandelbrot'}

def main():
    opts, args = gnu_getopt(argv[1:], 'l:t:c:n:o:p:h', ['libs=', 'tests=', 'threads=', 'normalizer=', 'offset=', 'precision=', 'help'])
    libs = None
    tests = None
    threads = None
    normalizer = None
    offset = None
    precision = None
    show_help = len(args) != 0

    for option, value in opts:
        if option in ('-l', '--libs'):
            libs = [lib.strip() for lib in value.split(',')]
        elif option in ('-t', '--tests'):
            tests = [test.strip() for test in value.split(',')]
        elif option in ('-c', '--threads'):
            threads = parse_ranges(value)
        elif option in ('-n', '--normalizer'):
            normalizer = float(value)
        elif option in ('-o', '--offset'):
            offset = float(value)
        elif option in ('-p', '--precision'):
            precision = int(value)
        elif option in ('-h', '--help'):
            show_help = True

    if show_help:
        stderr.write(
            'usage: statistics.py [OPTIONS]\n'
            'where OPTIONS are:\n'
            '-t,--tests      TESTS      comma-separated list of tests to plot\n'
            '-l,--libs       LIBS       comma-separated list of libs to plot\n'
            '-c,--threads    THREADS    thread-counts to include in a comma-separated list (e.g. 1-2,4)\n'
            '-n,--normalizer NORMALIZER Normalizer value\n'
            '-o,--offset     OFFSET     Offset/displacement for values\n'
            '-p,--precision  PRECISION  Precision.\n'
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

    make_stats(libs, tests, threads, normalizer, offset, precision)
    return 0

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

def make_stats(libs, tests, threads_list, normalizer=None, offset=None, precision=None):
    def single_file_stats(filename, columns):
        with open(filename, 'r') as f:
            lines = f.readlines()
        data = []
        for line in lines:
            assert line.endswith('\n')
            numbers = line[:-1].split(' ')
            numbers = [int(n) for n in numbers]
            if len(numbers) == 1:
                data.append(numbers[0])
            else:
                data.append(numbers[1] - numbers[0])
        if offset is not None:
            data = [x - offset for x in data]
        data = np.array(data)
        mean = np.mean(data)
        if normalizer is not None:
            mean = mean/normalizer
            data = [x/normalizer for x in data]
        low, high = np.percentile(data, [25, 75])
        mad = np.mean(np.absolute(data - mean))
        std = np.std(data)
        cols = [mean, high - low, mad, std]
        if precision is None:
            cols = ["%g" % x for x in cols]
        else:
            fmt = "%%.%df" % precision
            cols = [fmt % x for x in cols]
        columns.extend(cols)
        return mean

    rows = []
    for test in tests:
        for lib in libs:
            columns = []
            if len(lib) > 1:
                columns.append(lib)
            base_name = []
            if test in THREADED_TESTS:
                for threads in threads_list:
                    name = base_name[:]
                    filename = "results/%s-%s-%d.txt" % (test, lib, threads)
                    mean = single_file_stats(filename, columns)
            else:
                filename = "results/%s-%s.txt" % (test, lib)
                mean = single_file_stats(filename, columns)
            rows.append((mean, columns))

    rows = [r for _, r in sorted(rows)]
    rows.insert(0, ["Library", "Ticks", "IQR", "MAD", "Std deviation"])
    if normalizer is not None:
        rows[0][1] = "Relative time"

    colwidths = [0]*len(rows[0])
    for row in rows:
        widths = [len(x) for x in row]
        colwidths = [max(a, b) for a,b in zip(colwidths, widths)]
    first_row = True
    for row in rows:
        first_col = True
        for col, width in zip(row, colwidths):
            if not first_col:
                stdout.write(' | ')
            first_col = False
            stdout.write(' '*(width - len(col)))
            stdout.write(col)
        stdout.write('\n')
        if first_row:
            line = ['-'*w for w in colwidths]
            line = '-|-'.join(line) + '\n'
            stdout.write(line)
            first_row = False

if __name__ == "__main__":
    sys.exit(main())

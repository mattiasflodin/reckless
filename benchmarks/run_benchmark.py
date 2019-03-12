#!/usr/bin/python3

import sys, os, time, subprocess, errno
from sys import stdout, stderr, argv
from getopt import gnu_getopt

ALL_LIBS = ['nop', 'reckless', 'stdio', 'fstream', 'boost_log', 'spdlog', 'g3log']
ALL_TESTS = ['periodic_calls', 'call_burst', 'write_files', 'mandelbrot']

SINGLE_SAMPLE_TESTS = {'mandelbrot'}
THREADED_TESTS = {'call_burst', 'mandelbrot'}
TESTS_WITH_DRY_RUN = {'call_burst', 'periodic_calls'}
MAX_THREADS = 8

# /run_benchmark.py -t mandelbrot  1448.92s user 64.87s system 208% cpu 12:04.87 total
# with SINGLE_SAMPLE_TEST_ITERATIONS=2 means we should have
# about 80 for 8 hours runtime
#
# time ./run_benchmark.py -t mandelbrot
# mandelbrot: fstream/1234 nop/1234 pantheios/1234 reckless/1234 spdlog/1234 stdio/1234
# ./run_benchmark.py -t mandelbrot  58192.78s user 2484.45s system 208% cpu 8:04:15.16 total
#
# Then 100 = 10 hours runtime
SINGLE_SAMPLE_TEST_ITERATIONS = 100

def main():
    opts, args = gnu_getopt(argv[1:], 'l:t:h', ['libs', 'tests', 'help'])
    libs = None
    tests = None
    show_help = len(args) != 0

    for option, value in opts:
        if option in ('-l', '--libs'):
            libs = [lib.strip() for lib in value.split(',')]
        elif option in ('-t', '--tests'):
            tests = [test.strip() for test in value.split(',')]
        elif option in ('-h', '--help'):
            show_help = True

    if show_help:
        stderr.write(
            'usage: run_benchmark.py [OPTIONS]\n'
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

    run_tests(libs, tests)
    return 0

def run_tests(libs, tests):
    for test in tests:
        stdout.write(test + ':')
        stdout.flush()
        for lib in libs:
            stdout.write(' ' + lib)
            if test in THREADED_TESTS:
                stdout.write('/')
                for threads in range(1, MAX_THREADS+1):
                    stdout.write(str(threads))
                    stdout.flush()
                    success = run_test(lib, test, threads)
            else:
                stdout.flush()
                success = run_test(lib, test)

            if not success:
                stdout.write('[F]')

        stdout.write('\n')
        stdout.flush()

def reset():
    if os.path.exists('log.txt'):
        os.unlink('log.txt')
    for name in os.listdir('data'):
        os.unlink(os.path.join('data', name))
    subprocess.call('sync')

def run_test(lib, test, threads = None):
    binary_name = test + '-' + lib
    if threads is not None:
        binary_name += '-' + str(threads)

    def run(out_file):
        try:
            p = subprocess.Popen([binary_name], executable='./' + binary_name, stdout=out)
            p.wait()
        except OSError as e:
            if e.errno == errno.ENOENT:
                return False
            else:
                raise

    if test in TESTS_WITH_DRY_RUN:
        with open(os.devnull, 'w') as out:
            run(out)
    else:
        busy_wait(0.5)

    txt_name = binary_name + '.txt'
    with open('results/' + txt_name, 'w') as out:
        total_iterations = 1
        if test in SINGLE_SAMPLE_TESTS:
            total_iterations = SINGLE_SAMPLE_TEST_ITERATIONS
        for i in range(0, total_iterations):
            reset()
            run(out)
    return True

def busy_wait(period):
    end = time.time() + period
    while time.time() < end:
        pass

if __name__ == "__main__":
    sys.exit(main())

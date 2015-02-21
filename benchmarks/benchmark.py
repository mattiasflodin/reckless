#!/usr/bin/python3
import sys
from sys import stdout, stderr
import os
import subprocess

BENCH_PATH = os.path.dirname(os.path.realpath(__file__))
AVAILABLE_LIBS = ['asynclog', 'spdlog', 'pantheios', 'stdio', 'fstream']

def generate_test_cpps(libs, variants):
    for lib in AVAILABLE_LIBS:
        for variant in variants:
            path = os.path.join(BENCH_PATH, test_cpp_name(lib, variant))
            if lib in libs:
                if not os.path.exists(path):
                    with open(path, 'w') as f:
                        f.write('#include "{}.hpp"\n'
                            '#include "{}.hpp"\n'.format(lib, variant))
            else:
                if os.path.exists(path):
                    print("rm", path)
                    os.unlink(path)

def run(lib, variant, dry_run):
    cmd_name = lib + '_' + variant
    
    out = os.devnull #subprocess.DEVNULL
    if dry_run:
        out = open(os.devnull, 'w')
    else:
        txt_name = cmd_name + '.txt'
        out = open('results/' + txt_name, 'w')
    p = subprocess.Popen([cmd_name], executable='./' + cmd_name, stdout=out)
    p.wait()
    out.close()

def test_name(lib, variant):
    return lib + '_' + variant

def test_cpp_name(lib, variant):
    return test_name(lib, variant) + '.cpp'

libs = []
args_ok = True
for lib in sys.argv[1:]:
    if lib in AVAILABLE_LIBS or lib == 'all':
        libs.append(lib)
    else:
        sys.stderr("unknown lib '{}'\n".format(lib))
        args_ok = False
if len(libs) == 0:
    args_ok = False

if 'all' in libs:
    libs = AVAILABLE_LIBS
libs = sorted(list(set(libs)))

if not args_ok:
    stderr.write("usage: benchmark.py LIBS\n"
            "where LIBS is a list of the libraries to benchmark. Available choices:\n  {}\n"
            "or 'all' to benchmark all libs.\n".format('\n  '.join(AVAILABLE_LIBS)))
    sys.exit(1)

variants = ['periodic_calls', 'simple_call_burst']

if not os.path.isdir('results'):
    os.mkdir('results')

generate_test_cpps(libs, variants)
subprocess.check_call('tup')

for lib in libs:
    stdout.write(lib)
    stdout.flush()
    run(lib, variants[0], True)
    for variant in variants:
        if os.path.exists('log.txt'):
            os.unlink('log.txt')
        stdout.write(' ' + variant)
        stdout.flush()
        run(lib, variant, False)
    stdout.write('\n')

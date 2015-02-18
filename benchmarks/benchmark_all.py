#!/usr/bin/python3
from sys import stdout
import os
import subprocess

def run(lib, variant, dry_run):
    cmd_name = lib + '_' + variant
    out = subprocess.DEVNULL
    if not dry_run:
        txt_name = cmd_name + '.txt'
        out = open('results/' + txt_name, 'w')
    p = subprocess.Popen([cmd_name], executable='./' + cmd_name, stdout=out)
    p.wait()
    if not dry_run:
        out.close()

libs = ['asynclog', 'spdlog', 'pantheios']
variants = ['periodic_calls', 'simple_call_burst']

if not os.path.isdir('results'):
    os.mkdir('results')
    
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

#!/usr/bin/python3

import sys, os
import re

SOURCE_DIRS = ['reckless', 'examples', 'tests', 'performance_log']

ROOTDIR = os.path.relpath(os.path.join(os.path.dirname(__file__), '..'))

def main():
    read_license()
    for source_dir in SOURCE_DIRS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOTDIR, source_dir)):
            for filename in filenames:
                fullpath = os.path.join(dirpath, filename)
                fullpath = os.path.relpath(fullpath)
                if filename.endswith('.cpp') or filename.endswith('.hpp'):
                    update_license_header(fullpath)
    return 0

def read_license():
    global LICENSE

    license = os.path.join(ROOTDIR, 'scripts', 'license_header.txt')
    with open(license) as f:
        license = f.read()
    license = license.split('\n')
    if license[-1] == '':
        license = license[:-1]

    LICENSE = ['/* ' + license[0]] + [' * ' + x for x in license[1:]] + [' */']
    LICENSE = [x.rstrip() for x in LICENSE]
    LICENSE = '\n'.join(LICENSE) + '\n'

def update_license_header(path):
    with open(path) as f:
        original_data = f.read()
    data = original_data
    if data.startswith('/*'):
        i = data.find('*/\n')
        if i != -1:
            data = data[i+3:]
    data = LICENSE + data
    if original_data == data:
        return
    with open(path, 'w') as f:
        f.write(data)
        print(path)

if __name__ == '__main__':
    sys.exit(main())

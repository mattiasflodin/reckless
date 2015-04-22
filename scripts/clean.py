#!/usr/bin/python3
from sys import stdout
import os, errno
from os.path import join
import shutil
import subprocess

for root, dirs, files in os.walk('.'):
    assert root.startswith('.')
    if root.startswith('./'):
        root = root[2:]
    else:
        root = root[1:]
    if '.gitignore' not in files:
        continue
    
    files_to_remove = []
    preserved_lines = []
    with open(join(root, '.gitignore'), 'rU') as f:
        live = False
        for line in f.readlines():
            if line == "##### TUP GITIGNORE #####\n":
                live = True
            if live:
                if line.startswith('/') and line != '/.gitignore\n':
                    files_to_remove.append(line[1:-1])
            else:
                preserved_lines.append(line)

    if len(files_to_remove) != 0:
        stdout.write(root + '/\n')
    for name in files_to_remove:
        stdout.write('  ')
        stdout.write(name + '\n')
        try:
            os.unlink(join(root, name))
        except OSError as e:
            if e.errno == errno.ENOENT:
                pass
            else:
                raise
        
    if len(preserved_lines) == 0:
        os.unlink(join(root, '.gitignore'))
    else:
        with open(join(root, '.gitignore'), 'w') as f:
            f.write(''.join(preserved_lines))

shutil.rmtree(".tup")
subprocess.check_call(['tup', 'init'])

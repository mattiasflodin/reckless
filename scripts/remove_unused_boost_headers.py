#!/usr/bin/python3
# This performs a clean rebuild wrapped in strace to figure out which boost
# header files are actually used, then deletes any boost files that aren't
# needed.  Be careful running this since it will remove things that might
# actually be used on other platforms. It's meant as an aid to do the bulk work
# when adding new boost sources or changing boost version, and will require
# manual cleanup afterwards.

# TODO this needs to be updated to use tup instead of waf, since the wscript is gone
import subprocess
import re
import os

call_re = re.compile(br'\[pid\s+(\d+)\] (execve|open)\("([^"]+)"')

def main():
    subprocess.check_call(['./waf', 'clean'], executable='./waf')
    strace = subprocess.Popen(['strace', '-f', './waf'], executable='/usr/bin/strace', stderr=subprocess.PIPE)
    boost_path = os.path.join(os.getcwd(), 'include', 'boost_1_56_0')

    used_files = set()
    for line in strace.stderr:
        m = call_re.match(line)
        #print(line)
        if m is None:
            continue
        #print(line[:-1])
        pid, op, path = m.groups()
        path = path.decode('utf-8')
        if op == b'open':
            if not path.startswith(boost_path) or not (path.endswith('.hpp') or path.endswith('.h')):
                continue
            if not os.path.exists(path):
                continue
            path = path[len(boost_path)+1:]
            used_files.add(path)
    
    existing_files = set()
    for root, dirs, files in os.walk(boost_path):
        for name in files:
            path = os.path.join(root, name)
            path = path[len(boost_path)+1:]
            existing_files.add(path)
            #print(path)

    unused_files = existing_files.difference(used_files)
    print("Unused files:")
    for path in unused_files:
        os.unlink(os.path.join(boost_path, path))
        print(path)

    while True:
        empty_dirs = []
        for root, dirs, files in os.walk(boost_path):
            if len(files) == 0 and len(dirs) == 0:
                empty_dirs.append(root)
        if len(empty_dirs) == 0:
            break
        else:
            for path in empty_dirs:
                os.rmdir(os.path.join(boost_path, path))
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())

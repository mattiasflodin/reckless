#!/usr/bin/python3
# Run this in repository root with boost include files in include/boost_1_56_0.
# It will change all include directives so that they point to boost_1_56_0/
# instead of just boost/ to avoid name clashes. Boost should have been copied
# there with the BCP tool using the namespace renaming function, so that boost
# is defined in our own namespace.

import io
import re
import os

#include_re = re.compile(br'^(\s*#\s*include\s+[<"])boost(/)', re.MULTILINE)
include_re = re.compile(br'([<"(])boost/')

def main():
    boost_path = os.path.join(os.getcwd(), 'include', 'boost_1_56_0')
    print(boost_path)
    for root, dirs, files in os.walk(boost_path):
        for name in files:
            if not name.endswith('.hpp'):
                continue
            print(name)
            with open(os.path.join(root, name), 'rb+') as f:
                source = f.read()
                #source = include_re.sub(br'\1boost_1_56_0\2', source)
                source = include_re.sub(br'\1boost_1_56_0/', source)
                f.seek(0, io.SEEK_SET)
                f.write(source)
                #for m in include_re.finditer(source):
                #    print(b' ' + m.group(0))

    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())

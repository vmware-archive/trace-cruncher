#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2022 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import pydoc
import sys
import os

def main():
    path = os.path.abspath(sys.argv[0])
    d = os.path.dirname(path)
    os.chdir(d)

    module = sys.argv[1]
    pydoc.writedoc(module)

    doc_file = './' + module + '.html'
    with open(doc_file, 'rt') as f:
        data = f.read()
        data = data.replace('#ee77aa', '#5577aa')
        data = data.replace('#ffc8d8', '66c8d8')
        data = data.replace('#eeaa77', '55aa77')

    with open(doc_file, 'wt') as f:
        f.write(data)

    if len(sys.argv) == 4:
        os.chown(doc_file, int(sys.argv[2]), int(sys.argv[3]))


if __name__ == '__main__':
    main()

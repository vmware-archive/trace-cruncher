#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
"""

import os
import sys

def setup():
    os.chdir(os.path.dirname(__file__))
    path = os.getcwd() + '/..'
    sys.path.append(path)

    if 'LD_LIBRARY_PATH' not in os.environ:
        os.environ['LD_LIBRARY_PATH'] = '/usr/local/lib/kernelshark:/usr/local/lib/traceevent:/usr/local/lib/trace-cmd'
        try:
            os.execv(sys.argv[0], sys.argv)
        except e:
            print('Failed re-exec:', e)
            sys.exit(1)


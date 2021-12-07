#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import sys
import time

import tracecruncher.ftracepy as ft

inst_name = 'khist_example'

cmds = ['start', 'stop', 'show', 'continue', 'clear', 'close']

def get_hist():
    hist = ft.hist(name='h1',
                   system='kmem',
                   event='kmalloc',
                   axes={'call_site': 'sym',
                         'bytes_req': 'n'})

    hist.add_value(value='bytes_alloc')
    hist.sort_keys(keys=['bytes_req', 'bytes_alloc'])
    hist.sort_key_direction(sort_key='bytes_req', direction='desc')

    return hist

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(1)

    if not sys.argv[1].isdigit() and not sys.argv[1] in cmds:
        sys.exit(1)

    arg1 = sys.argv[1]
    if  arg1.isdigit() or arg1 == 'start':
        inst = ft.create_instance(name=inst_name)
        hist = get_hist()
        hist.start(inst)

        if arg1.isdigit():
            time.sleep(int(arg1))
            hist.stop(inst)
            print(hist.read(inst))
            hist.close(inst)
        else:
            ft.detach(inst)
    else:
        inst = ft.find_instance(name=inst_name)
        hist = get_hist()

        if arg1 == 'stop':
            hist.stop(inst)
        elif arg1 == 'show':
            print(hist.read(inst))
        elif arg1 == 'continue':
            hist.resume(inst)
        elif arg1 == 'clear':
            hist.clear(inst)

        if arg1 == 'close':
            ft.attach(inst)
            hist.close(inst)

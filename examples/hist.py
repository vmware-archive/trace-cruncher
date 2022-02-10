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
    # From the event 'kmalloc' in system 'kmem', create a two-dimensional
    # histogram, using the event fields 'call_site' and 'bytes_req'.
    #
    # The field 'call_site' will be displayed as a kernel symbol.
    # The field 'bytes_req' will be displayed as normal field (wothout
    # modifying the type).
    #
    # Instead of just recording the "hitcount" in each bin of the histogram,
    # we will use the 'value' of 'bytes_alloc' as a weight of the individual
    # histogram entries (events).
    #
    # The results will be ordered using 'bytes_req' as a primary and
    # 'bytes_alloc' as a secondary sorting criteria. For 'bytes_req' we will
    # use descending order.
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
        # Create new Ftrace instance and a tracing histogram.
        inst = ft.create_instance(name=inst_name)
        hist = get_hist()

        # Start taking data.
        hist.start(inst)

        if arg1.isdigit():
            # Take data for a while, then stop, print the result, close
            # the histogram and exit.
            time.sleep(int(arg1))
            hist.stop(inst)
            print(hist.read(inst))
            hist.close(inst)
        else:
            # Detach the 'hist' object from the trace-cruncher module. This
            # will prevent the kernel histogram from being destroyed when the
            # module is closed (at exit).
            ft.detach(inst)
    else:
        # Try to find existing Ftrace instance and histogram with the same
        # definitions. The returned instancd is detached from the
        # trace-cruncher module.
        inst = ft.find_instance(name=inst_name)
        hist = get_hist()

        if arg1 == 'stop':
            # Stop taking data.
            hist.stop(inst)
        elif arg1 == 'show':
            # Print the collected data.
            print(hist.read(inst))
        elif arg1 == 'continue':
            # Continue taking data.
            hist.resume(inst)
        elif arg1 == 'clear':
            # Reset the histogram.
            hist.clear(inst)

        if arg1 == 'close':
            # Destroy the histogram in the kernel and attach the instance to
            # the trace-cruncher module. This will ensure that the instance
            # will be destroyed when the module is closed (at exit).
            ft.attach(inst)
            hist.close(inst)

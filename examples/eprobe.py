#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2022 VMware Inc, Tzvetomir Stoyanov (VMware) <tz.stoyanov@gmail.com>
"""

import sys

import tracecruncher.ftracepy as ft
import tracecruncher.ft_utils as tc

open_probe = ft.eprobe(event='sopen_in', target_system='syscalls',
                       target_event='sys_enter_openat', fetchargs='file=+0($filename):ustring')

tep = tc.local_tep()

def callback(event, record):
    print(tep.info(event, record))

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print('Usage: ', sys.argv[0], ' [PROCESS]')
        sys.exit(1)

    inst = ft.create_instance(tracing_on=False)
    open_probe.enable(instance=inst)
    ft.trace_process(instance=inst, argv=sys.argv[1:])

#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import sys

import tracecruncher.ftracepy as ft
import tracecruncher.ft_utils as tc

open_probe = tc.kprobe(name='open', func='do_sys_openat2')

open_probe.add_string_arg(name='file', param_id=2)

open_probe.add_ptr_arg(name='flags',
                       param_id=3,
                       param_type='x64')

open_probe.add_ptr_arg(name='mode',
                       param_id=3,
                       param_type='x64',
                       offset=8)

open_probe.register()

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

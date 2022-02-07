#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import sys

import tracecruncher.ftracepy as ft
import tracecruncher.ft_utils as tc

# We will define a kprobe event, to be recorded every time the function
# 'do_sys_openat2' in the kernrl is called.

# Add to the kprobe event a string field 'file', that will record the second
# argument of the function.
fields = tc.kprobe_add_string_arg(name='file', param_id=2)

# Add to the kprobe event a field 'flags'. This field will take the third
# argument of the function, which is a pointer to an object and will convert
# its starting address into hexadecimal integer. This will decode the first
# data field of the object.
fields = tc.kprobe_add_ptr_arg(name='flags',
                               param_id=3,
                               param_type='x64',
                               fields=fields)

# Add to the kprobe event a field 'flags'. This field will take the third
# argument of the function, which is a pointer to an object and will convert
# its starting address plus offset of 8 into hexadecimal integer. This will
# decode the second data field of the object.
fields = tc.kprobe_add_ptr_arg(name='mode',
                               param_id=3,
                               param_type='x64',
                               offset=8,
                               fields=fields)

# Create the kprobe event.
open_probe = tc.kprobe(name='open',
                       func='do_sys_openat2',
                       fields=fields)

tep = tc.local_tep()
tc.short_kprobe_print(tep, [open_probe])

# Define a callback function that will print
# a short human-readable version.
def callback(event, record):
    print(tep.info(event, record))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print('Usage: ', sys.argv[0], ' [PROCESS]')
        sys.exit(1)

    # Create new Ftrace instance to work in. The tracing in this new instance
    # is not going to be enabled yet.
    inst = ft.create_instance(tracing_on=False)

    # Enable the probe.
    open_probe.enable(instance=inst)

    # Subscribe for the kprobe event (using the default function name 'callback')
    # and trace the user process.
    ft.trace_process(instance=inst, argv=sys.argv[1:])

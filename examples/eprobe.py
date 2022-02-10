#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2022 VMware Inc, Tzvetomir Stoyanov (VMware) <tz.stoyanov@gmail.com>
"""

import sys

import tracecruncher.ftracepy as ft
import tracecruncher.ft_utils as tc

# Create an 'eprobe' that will be attached to the static event 'sys_enter_openat'
# from system 'syscalls'. The probe will decode the string field of the event
# called 'filename' and will record its content using the name 'file'.
fields = tc.eprobe_add_string_field(name='file', target_field='filename',
                                    usr_space=True)
event = tc.tc_event('syscalls', 'sys_enter_openat')
eprobe = tc.tc_eprobe(name='sopen_in', target_event=event, fields=fields)

# Define a callback function that will print
# a short human-readable version of the 'eprobe'.

tep = tc.local_tep()
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
    eprobe.enable(instance=inst)

    # Subscribe for the kprobe event (using the default function name 'callback')
    # and trace the user process.
    ft.trace_process(instance=inst, argv=sys.argv[1:])

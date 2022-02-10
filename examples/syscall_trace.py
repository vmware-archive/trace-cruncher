#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2022 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import sys

import tracecruncher.ftracepy as ft
import tracecruncher.ft_utils as tc

eprobe_evt = 'eprobe_open'
synth_evt = 'synth_open'
syscall = 'openat'
args = 'file=+0($file):ustring delta_T=$delta_T:s64'

# In order to trace a system call, we will create a synthetic event that
# combines the 'sys_enter_XXX' and 'sys_exit_XXX' static events. A dynamic
# 'eprobe' will be attached to this synthetic event in order to decode the
# pointer argument of the system and to calculate the time spend between
# 'sys_enter_XXX' and 'sys_exit_XXX' (syscall duration).

eprobe = ft.eprobe(event=eprobe_evt,
                   target_system='synthetic', target_event=synth_evt,
                   fetch_fields=args)

synth = ft.synth(name=synth_evt,
                 start_sys='syscalls', start_evt='sys_enter_' + syscall,
                 end_sys='syscalls',   end_evt='sys_exit_' + syscall,
                 start_match='common_pid', end_match='common_pid')

# Add to the synth. event one field from the 'start' event. In the synth. event,
# the field 'filename' will be renamed to 'file'.
synth.add_start_fields(fields=['filename'], names=['file'])

# Add to the synth. event a field that measures the time-difference between
# the 'start' and 'end' events. Use 'hd' time resolution (nanoseconds).
synth.add_delta_T(hd=True)

# The synthetic event must be registered first (and destroyed last), because the
# eprobe depends on it. Note that the order in which the events are allocated
# will be the order in which Python will destroy them at exit.
synth.register()
eprobe.register()

tep = tc.local_tep()
eprobe_id = tep.get_event(system=ft.tc_event_system(), name=eprobe_evt).id()

def callback(event, record):
    if event.id() == eprobe_id:
        # Print only the dynamic eprobe.
        print(tep.info(event, record))

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print('Usage: ', sys.argv[0], ' [PROCESS]')
        sys.exit(1)

    inst = ft.create_instance(tracing_on=False)

    # Enable the two events and trace the user process.
    synth.enable(instance=inst)
    eprobe.enable(instance=inst)
    ft.trace_process(instance=inst, argv=sys.argv[1:])

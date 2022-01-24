#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import tracecruncher.ftracepy as ft

# Define a synthetic event that combines 'sched_waking' and 'sched_switch'.
# A synth. event will be recorded every time a 'start' event (sched_waking)
# is followed by an 'end' event (sched_switch) and both events have the same
# value of the fields 'pid' and 'next_pid' (belong to the same process).
synth = ft.synth(name='synth_wakeup',
                 start_sys='sched', start_evt='sched_waking',
                 end_sys='sched',   end_evt='sched_switch',
                 start_match='pid', end_match='next_pid',
                 match_name='pid')

# Add to the synth. event two fields from the 'start' event. In the synth. event,
# the field 'target_cpu' will be renamed to 'cpu'.
synth.add_start_fields(fields=['target_cpu', 'prio'],
                       names=['cpu', None])

# Add to the synth. event one field from the 'end' event.
synth.add_end_fields(fields=['next_prio'])

# Add to the synth. event a field that measures the time-difference between
# the 'start' and 'end' events. Use 'hd' time resolution (nanoseconds).
synth.add_delta_T(hd=True)

# Register the synth. event on the system.
synth.register()

inst = ft.create_instance()

# Apply a filter and enable the synth. event.
synth.set_filter(instance=inst, filter='prio<100')
synth.enable(instance=inst)

# Print the stream of trace events. "Ctrl+c" to stop tracing.
ft.read_trace(instance=inst)

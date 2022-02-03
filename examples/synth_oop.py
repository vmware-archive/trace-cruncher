#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import tracecruncher.ftracepy as ft
import tracecruncher.ft_utils as tc

# We will define a synthetic event, to be recorded every time a 'start' event is
# followed by an 'end' event and both events have the same value of a 'match' field.

# Get the static kernel event 'sched_waking' and 'sched_switch' from system
# 'sched'. Those two events will be used to create the synthetic event.
swaking = tc.event('sched', 'sched_waking')
sswitch = tc.event('sched', 'sched_switch')

# Add to the synth. event two fields from the 'start' event (sched_waking). In the
# synth. event, the field 'target_cpu' will be renamed to 'cpu'. Use the 'pid' field
# for matching with the corresponding 'end' event.
start = tc.ksynth_event_item(event=swaking, fields=['target_cpu', 'prio'], match='pid')
start = tc.ksynth_field_rename(start, field='target_cpu', name='cpu')

# Add to the synth. event one field from the 'end' event (sched_switch).
# Use the 'next_pid' field for matching with the corresponding 'start' event.
end = tc.ksynth_event_item(event=sswitch, fields=['prev_prio'], match='next_pid')

# Define a synthetic event. The 'match' value will be recorder as a field in the
# synth. event using 'pid' as name. We also add to the synth. event a field that
# measures the time-difference between the 'start' and 'end' events. This new field
# will use 'hd' time resolution (nanoseconds).
synth = tc.ksynth(name='synth_wakeup', start_event=start, end_event=end,
                  synth_fields=[tc.ksynth_field_deltaT(hd=True)], match_name='pid')

# Create new instance of Ftrace.
inst = ft.create_instance()

# Apply a filter and enable the synth. event.
synth.set_filter(instance=inst, filter='prio<100')
synth.enable(instance=inst)

# Print the stream of trace events. "Ctrl+c" to stop tracing.
ft.read_trace(instance=inst)

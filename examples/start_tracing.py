#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import tracecruncher.ftracepy as ft

# Create new Ftrace instance to work in.
inst = ft.create_instance()

# Enable several static events, including "sched_switch" and "sched_waking"
# from systems "sched" and all events from system "irq".
ft.enable_events(instance=inst,
                 events={'sched': ['sched_switch', 'sched_waking'],
                         'irq':   ['all']})

# Print the stream of trace events. "Ctrl+c" to stop tracing.
ft.read_trace(instance=inst)

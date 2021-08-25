#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import tracecruncher.ftracepy as ft

# Create new Ftrace instance to work in.
inst = ft.create_instance()

# Enable all static events from systems "sched" and "irq".
ft.enable_events(instance=inst,
                 systems=['sched', 'irq'],
                 events=[['sched_switch'],['all']])

# Print the stream of trace events. "Ctrl+c" to stop tracing.
ft.read_trace(instance=inst)

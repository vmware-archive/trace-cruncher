#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
"""

import os
import sys
import json
import pprint as pr

import matplotlib.pyplot as plt
import scipy.stats as st
import numpy as np

import tracecruncher.ks_utils as tc

# Get the name of the user program.
if len(sys.argv) >= 2:
    fname = str(sys.argv[1])
else:
    fname = input('choose a trace file: ')

f = tc.open_file(file_name=fname)

# We do not need the Process Ids of the records.
# Do not load the "pid" data.
data = f.load(pid_data=False)
tasks = f.get_tasks()

# Get the name of the user program.
if len(sys.argv) >= 3:
    prog_name = str(sys.argv[2])
else:
    pr.pprint(tasks)
    prog_name = input('choose a task: ')

task_pid = tasks[prog_name][0]

# Get the Event Ids of the sched_switch and sched_waking events.
ss_eid = f.event_id(name='sched/sched_switch')
w_eid = f.event_id(name='sched/sched_waking')

# Gey the size of the data.
i = tc.size(data)

dt = []
delta_max = i_ss_max = i_sw_max = 0

while i > 0:
    i = i - 1
    if data['event'][i] == ss_eid:
        next_pid = f.read_event_field(offset=data['offset'][i],
                                       event_id=ss_eid,
                                       field='next_pid')

        if next_pid == task_pid:
            time_ss = data['time'][i]
            index_ss = i
            cpu_ss = data['cpu'][i]

            while i > 0:
                i = i - 1

                if data['event'][i] < 0 and cpu_ss == data['cpu'][i]:
                        # Ring buffer overflow. Ignore this case and continue.
                        break

                if data['event'][i] == ss_eid:
                    next_pid = f.read_event_field(offset=data['offset'][i],
                                                  event_id=ss_eid,
                                                  field='next_pid')
                    if next_pid == task_pid:
                        # Second sched_switch for the same task. ?
                        time_ss = data['time'][i]
                        index_ss = i
                        cpu_ss = data['cpu'][i]

                    continue

                if (data['event'][i] == w_eid):
                    waking_pid = f.read_event_field(offset=data['offset'][i],
                                                     event_id=w_eid,
                                                     field='pid')

                    if waking_pid == task_pid:
                        delta = (time_ss - data['time'][i]) / 1000.
                        dt.append(delta)
                        if delta > delta_max:
                            print('lat. max: ', delta)
                            i_ss_max = index_ss
                            i_sw_max = i
                            delta_max = delta

                        break

desc = st.describe(np.array(dt))
print(desc)

# Plot the latency distribution.
fig, ax = plt.subplots(nrows=1, ncols=1)
fig.set_figheight(6)
fig.set_figwidth(7)

rect = fig.patch
rect.set_facecolor('white')

ax.set_xlabel('latency [$\mu$s]')
#plt.yscale('log')
ax.hist(dt, bins=(100), histtype='step')
plt.show()

# Prepare a session description for KernelShark.
s = tc.ks_session('sched')

delta = data['time'][i_ss_max] - data['time'][i_sw_max]
tmin = data['time'][i_sw_max] - delta
tmax = data['time'][i_ss_max] + delta

s.set_time_range(tmin=tmin, tmax=tmax)

cpu_plots = [data['cpu'][i_sw_max]]
if data['cpu'][i_ss_max] != data['cpu'][i_sw_max]:
    cpu_plots.append(data['cpu'][i_ss_max])

s.set_cpu_plots(f, cpu_plots)
s.set_task_plots(f, [task_pid])

s.set_marker_a(i_sw_max)
s.set_marker_b(i_ss_max)

s.set_first_visible_row(i_sw_max - 5)

s.add_plugin(stream=f, plugin='sched_events')

s.save()

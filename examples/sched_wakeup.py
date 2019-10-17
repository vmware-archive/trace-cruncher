#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
"""

import os
import sys
import json

import matplotlib.pyplot as plt
import scipy.stats as st
import numpy as np

from ksharksetup import setup
# Always call setup() before importing ksharkpy!!!
setup()

import ksharkpy as ks

fname = str(sys.argv[1])

ks.open_file(fname)

# We do not need the Process Ids of the records.
# Do not load the "pid" data.
data = ks.load_data(pid_data=False)

# Get the name of the user program.
prog_name = str(sys.argv[2])

tasks = ks.get_tasks()
task_pid = tasks[prog_name]

# Get the Event Ids of the sched_switch and sched_waking events.
ss_eid = ks.event_id('sched', 'sched_switch')
w_eid = ks.event_id('sched', 'sched_waking')

# Gey the size of the data.
i = data['offset'].size

dt = []
delta_max = i_ss_max = i_sw_max = 0

while i > 0:
    i = i - 1
    if data['event'][i] == ss_eid:
        next_pid = ks.read_event_field(offset=data['offset'][i],
                                       event_id=ss_eid,
                                       field='next_pid')

        if next_pid == task_pid:
            time_ss = data['time'][i]
            index_ss = i

            while i > 0:
                i = i - 1
                if (data['event'][i] == w_eid):
                    waking_pid = ks.read_event_field(offset=data['offset'][i],
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

fig, ax = plt.subplots(nrows=1, ncols=1)
fig.set_figheight(6)
fig.set_figwidth(7)

rect = fig.patch
rect.set_facecolor('white')

ax.set_xlabel('latency [$\mu$s]')
plt.yscale('log')
ax.hist(dt, bins=(200), histtype='step')
plt.show()

sname = 'sched.json'
ks.new_session(fname, sname)

with open(sname, 'r+') as s:
    session = json.load(s)
    session['TaskPlots'] = [task_pid]
    session['CPUPlots'] = [
        int(data['cpu'][i_sw_max]),
        int(data['cpu'][i_ss_max])]

    delta = data['time'][i_ss_max] - data['time'][i_sw_max]
    tmin = int(data['time'][i_sw_max] - delta)
    tmax = int(data['time'][i_ss_max] + delta)
    session['Model']['range'] = [tmin, tmax]

    session['Markers']['markA']['isSet'] = True
    session['Markers']['markA']['row'] = int(i_sw_max)

    session['Markers']['markB']['isSet'] = True
    session['Markers']['markB']['row'] = int(i_ss_max)

    session['ViewTop'] = int(i_sw_max) - 5

    ks.save_session(session, s)

ks.close()

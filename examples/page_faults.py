#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
"""

import os
import sys
import subprocess as sp
import json

import pprint as pr
import matplotlib.pyplot as plt
import scipy.stats as st
import numpy as np
from collections import Counter
from tabulate import tabulate

from ksharksetup import setup
# Always call setup() before importing ksharkpy!!!
setup()

import ksharkpy as ks

def gdb_decode_address(obj_file, obj_address):
    """ Use gdb to examine the contents of the memory at this
        address.
    """
    result = sp.run(['gdb',
                     '--batch',
                     '-ex',
                     'x/i ' + str(obj_address),
                     obj_file],
                    stdout=sp.PIPE)

    symbol = result.stdout.decode("utf-8").splitlines()

    if symbol:
        func = [symbol[0].split(':')[0], symbol[0].split(':')[1]]
    else:
        func = [obj_address]

    func.append(obj_file)

    return func

# Get the name of the tracing data file.
fname = str(sys.argv[1])

ks.open_file(fname)
ks.register_plugin('reg_pid')

data = ks.load_data()
tasks = ks.get_tasks()
#pr.pprint(tasks)

# Get the Event Ids of the page_fault_user or page_fault_kernel events.
pf_eid = ks.event_id('exceptions', 'page_fault_user')

# Gey the size of the data.
d_size = ks.data_size(data)

# Get the name of the user program.
prog_name = str(sys.argv[2])

table_headers = ['N p.f.', 'function', 'value', 'obj. file']
table_list = []

# Loop over all tasks associated with the user program.
for j in range(len(tasks[prog_name])):
    count = Counter()
    task_pid = tasks[prog_name][j]
    for i in range(0, d_size):
        if data['event'][i] == pf_eid and data['pid'][i] == task_pid:
            address = ks.read_event_field(offset=data['offset'][i],
                                          event_id=pf_eid,
                                          field='address')
            ip = ks.read_event_field(offset=data['offset'][i],
                                     event_id=pf_eid,
                                     field='ip')
            count[ip] += 1

    pf_list = count.items()

    # Sort the counters of the page fault instruction pointers. The most
    # frequent will be on top.
    pf_list = sorted(pf_list, key=lambda cnt: cnt[1], reverse=True)

    i_max = 25
    if i_max > len(pf_list):
        i_max = len(pf_list)

    for i in range(0, i_max):
        func = ks.get_function(pf_list[i][0])
        func_info = [func]
        if func.startswith('0x'):
            # The name of the function cannot be determined. We have an
            # instruction pointer instead. Most probably this is a user-space
            # function.
            address = int(func, 0)
            instruction = ks.map_instruction_address(task_pid, address)

            if instruction['obj_file'] != 'UNKNOWN':
                func_info = gdb_decode_address(instruction['obj_file'],
                                               instruction['address'])
            else:
                func_info += ['', instruction['obj_file']]

        else:
            func_info = [func]

        table_list.append([pf_list[i][1]] + func_info)

ks.close()

print("\n", tabulate(table_list,
                     headers=table_headers,
                     tablefmt='simple'))

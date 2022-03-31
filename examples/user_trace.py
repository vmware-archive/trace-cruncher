#!/usr/bin/env python3

"""
SPDX-License-Identifier: CC-BY-4.0

Copyright 2022 VMware Inc, Tzvetomir Stoyanov (VMware) <tz.stoyanov@gmail.com>
"""

import sys
import shutil

import tracecruncher.ftracepy as ft

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print('Usage: ', sys.argv[0], ' [PROCESS or PID]')
        sys.exit(1)

    # Create new Ftrace instance to work in. The tracing in this new instance
    # is not going to be enabled yet.
    inst = ft.create_instance(tracing_on=False)

    # Create a user tracing context for given process, exclude the libraries
    if sys.argv[1].isdigit():
        utrace = ft.user_trace(pid=int(sys.argv[1]), follow_libs=False)
    else:
        utrace = ft.user_trace(argv=sys.argv[1:], follow_libs=False)

    # Trace execution of all available functions in the given process
    utrace.add_function(fname="*")

    # Add trace points on functions return as well
    utrace.add_ret_function(fname="*")

    # Start tracing in an instance
    utrace.enable(instance=inst, wait=False)

    # Read the trace buffer during the trace until ctrl-c is pressed
    ft.read_trace(instance=inst)

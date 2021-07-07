"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import sys
import time

from . import ftracepy as ft


def find_event_id(system, event):
    """ Get the unique identifier of a trace event.
    """
    tep = ft.tep_handle();
    tep.init_local(dir=ft.dir(), systems=[system]);

    return tep.get_event(system=system, name=event).id()

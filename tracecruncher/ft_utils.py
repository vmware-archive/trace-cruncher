"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import sys
import time
import ctypes

from . import ftracepy as ft


def local_tep():
    """ Get the "tep" event of the current system (local).
    """
    tep = ft.tep_handle();
    tep.init_local(dir=ft.dir());

    return tep


def find_event_id(system, event):
    """ Get the unique identifier of a trace event.
    """
    tep = ft.tep_handle();
    tep.init_local(dir=ft.dir(), systems=[system]);

    return tep.get_event(system=system, name=event).id()


class event:
    def __init__(self, system, name, static=True):
        """ Constructor.
        """
        self.system = system
        self.name = name
        self.instance_list = []
        if static:
            self.evt_id = find_event_id(system, name)
        else:
            self.evt_id = -1

    def id(self):
        """ Retrieve the unique ID of the kprobe event.
        """
        return int(self.evt_id)

    def enable(self, instance=None):
        """ Enable this event.
        """
        if instance is None:
            ft.enable_event(system=self.system, event=self.name)
            self.instance_list.append('top')
        else:
            ft.enable_event(instance=instance, system=self.system, event=self.name)
            self.instance_list.append(instance)

        self.instance_list = list(set(self.instance_list))

    def disable(self, instance=None):
        """ Disable this event.
        """
        if instance is None:
            ft.disable_event(system=self.system, event=self.name)
            self.instance_list.remove('top')
        else:
            ft.disable_event(instance=instance,system=self.system, event=self.name)
            self.instance_list.remove(instance)

    def set_filter(self, filter, instance=None):
        """ Define a filter for this event.
        """
        if instance is None:
            ft.set_event_filter(system=self.system,
                                event=self.name,
                                filter=filter)
        else:
            ft.set_event_filter(instance=instance,
                                system=self.system,
                                event=self.name,
                                filter=filter)

    def clear_filter(self, instance=None):
        """ Define the filter for this event.
        """
        if instance is None:
            ft.clear_event_filter(system=self.system,
                                  event=self.name)
        else:
            ft.clear_event_filter(instance=instance,
                                  system=self.system,
                                  event=self.name)


class kprobe_base(event):
    def __init__(self, name, func=''):
        """ Constructor.
        """
        super().__init__(system=ft.tc_event_system(), name=name, static=False)
        self.func = func

    def set_function(self, name):
        """ Set the name of the function to be traced.
        """
        self.func = name

    def unregister(self):
        """ Unregister this probe from Ftrace.
        """
        inst_list = self.instance_list.copy()
        for instance in inst_list:
            self.disable(instance)

        ft.unregister_kprobe(event=self.name);


class kprobe(kprobe_base):
    def __init__(self, name, func=''):
        """ Constructor.
        """
        super().__init__(name, func)
        self.fields = {}

    def add_raw_field(self, name, probe):
        """ Add a raw definition of a data field to this probe.
        """
        self.fields[str(name)] = str(probe)

    def add_arg(self, name, param_id, param_type):
        """ Add a function parameter data field to this probe.
        """
        probe = '$arg{0}:{1}'.format(param_id, param_type)
        self.add_raw_field(name, probe)

    def add_ptr_arg(self, name, param_id, param_type, offset=0):
        """ Add a pointer function parameter data field to this probe.
        """
        probe = '+{0}($arg{1}):{2}'.format(offset, param_id, param_type)
        self.add_raw_field(name, probe)

    def add_array_arg(self, name, param_id, param_type, offset=0, size=-1):
        """ Add a array parameter data field to this probe.
        """
        if size < 0:
            size = 10

        ptr_size = ctypes.sizeof(ctypes.c_voidp)
        for i in range(size):
            field_name = name + str(i)
            probe = '+{0}(+{1}'.format(offset, i * ptr_size)
            probe += '($arg{0})):{1}'.format(param_id, param_type)
            self.add_raw_field(field_name, probe)

    def add_string_arg(self, name, param_id, offset=0, usr_space=False):
        """ Add a pointer function parameter data field to this probe.
        """
        p_type = 'ustring' if usr_space else 'string'
        self.add_ptr_arg(name=name,
                         param_id=param_id,
                         param_type=p_type,
                         offset=offset)

    def add_string_array_arg(self, name, param_id, offset=0, usr_space=False, size=-1):
        """ Add a string array parameter data field to this probe.
        """
        p_type = 'ustring' if usr_space else 'string'
        self.add_array_arg(name=name,
                           param_id=param_id,
                           param_type=p_type,
                           offset=offset,
                           size=size)

    def register(self):
        """ Register this probe to Ftrace.
        """
        probe = ' '.join('{!s}={!s}'.format(key,val) for (key, val) in self.fields.items())

        ft.register_kprobe(event=self.name, function=self.func, probe=probe);
        self.evt_id = find_event_id(system=ft.tc_event_system(), event=self.name)


def parse_record_array_field(event, record, field, size=-1):
    """ Register this probe to Ftrace.
    """
    if size < 0:
        size = 10

    arr = []
    for i in range(size):
        field_name = field + str(i)
        val = event.parse_record_field(record=record, field=field_name)
        if (val == '(nil)'):
            break
        arr.append(val)

    return arr


class kretval_probe(kprobe_base):
    def __init__(self, name, func=''):
        """ Constructor.
        """
        super().__init__(name, func)

    def register(self):
        """ Register this probe to Ftrace.
        """
        ft.register_kprobe(event=self.name, function=self.func);
        self.evt_id = find_event_id(system=ft.tc_event_system(), event=self.name)

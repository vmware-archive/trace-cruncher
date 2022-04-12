"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import sys
import time
import ctypes

from . import ftracepy as ft


def local_tep():
    """
    Get the "tep" event of the current system (local).

    Returns
    -------
    object : PyTep
        Local tep handle.
    """
    tep = ft.tep_handle()
    tep.init_local(dir=ft.dir())

    return tep


def find_event_id(system, event):
    """
    Get the unique identifier of a trace event.

    Parameters
    ----------
    system : string
        The system of the event.
    event : string
        The name of the event.

    Returns
    -------
    id : int
        Id number.
    """
    tep = ft.tep_handle()
    tep.init_local(dir=ft.dir(), systems=[system])

    return tep.get_event(system=system, name=event).id()


def short_kprobe_print(tep, events):
    """
    Register short (no probe address) print for these kprobe events.

    Parameters
    ----------
    tep : PyTep
        Local tep handle.
    events : list of PyTepEvent
        List of kprobe events.
    """
    for e in events:
        if len(e.fields):
            tep.short_kprobe_print(id=e.evt_id, system=e.system, event=e.name)


class tc_event:
    """
    A class used to represent a tracing event.

    Attributes
    ----------
    system : string
        The system of the event.
    event:
        The name of the event.
    id : int
        The unique Id of the event.
    """
    def __init__(self, system, name, static=True):
        """
        Constructor

        Parameters
        ----------
        system : string
            The system of the event.
        event : string
            The name of the event.
        static : bool
            Is it a static or dynamic event.
        """
        self.system = system
        self.name = name
        if static:
            self.evt_id = find_event_id(system, name)
            if self.evt_id < 0:
                raise ValueError('Failed to find event {0}/{1}'.format(system, name))
        else:
            self.evt_id = -1

    def id(self):
        """
        Retrieve the unique ID of the event.
        """
        return int(self.evt_id)

    def enable(self, instance=ft.no_arg()):
        """
        Enable this event.

        Parameters
        ----------
        instance : PyTfsInstance (optional)
            The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.
        """
        ft.enable_event(instance=instance, system=self.system, event=self.name)

    def disable(self, instance=ft.no_arg()):
        """
        Disable this event.

        Parameters
        ----------
        instance : PyTfsInstance (optional)
            The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.
        """
        ft.disable_event(instance=instance,system=self.system, event=self.name)

    def set_filter(self, filter, instance=ft.no_arg()):
        """
        Define a filter for this event.

        Parameters
        ----------
        filter : string
            The filter descriptor.
        instance : PyTfsInstance (optional)
            The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.
        """
        ft.set_event_filter(instance=instance,
                            system=self.system,
                            event=self.name,
                            filter=filter)

    def clear_filter(self, instance=ft.no_arg()):
        """
        Clear the filter for this event.

        Parameters
        ----------
        instance : PyTfsInstance (optional)
            The Ftrace instance. This argument is optional. If not provided, the 'top' instance is used.
        """
        ft.clear_event_filter(instance=instance,
                              system=self.system,
                              event=self.name)


class _dynevent(tc_event):
    """
    A class used to represent a dynamic tracing event.

    Attributes
    ----------
    evt : PyDynevent
        Low-level dynamic event object.
    fields : dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    def __init__(self, name):
        """
        Constructor

        Parameters
        ----------
        name : string
            The name of the event.
        """
        super().__init__(system=ft.tc_event_system(), name=name, static=False)
        self.evt = None
        self.fields = None
        self.evt_id = -1

    def register(self):
        """
        Register this probe to Ftrace.
        """
        self.evt.register()
        self.evt_id = find_event_id(system=ft.tc_event_system(), event=self.name)


class _kprobe_base(_dynevent):
    """
    Base class used to represent a kprobe or kretval probe.

    Attributes
    ----------
    func : string
        Name of a function or address to attach the probe to.
    """
    def __init__(self, name, func):
        """
        Constructor.

        Parameters
        ----------
        name : string
            The name of the kprobe event.
        func: string
            Name of a function or address.
        """
        super().__init__(name=name)
        self.func = func


class tc_kprobe(_kprobe_base):
    """
    A class used to represent a kprobe event.
    """
    def __init__(self, name, func, fields):
        """
        Constructor

        Parameters
        ----------
        name : string
            The name of the kprobe event.
        func: string
            Name of a function or address.
        fields: dictionary (string : string)
            Key - kprobe field name, value - definition of the probe.
        """
        super().__init__(name, func)
        self.fields = fields
        probe = ' '.join('{!s}={!s}'.format(key, val) for (key, val) in self.fields.items())
        self.evt = ft.kprobe(event=self.name, function=self.func, probe=probe)
        self.register()


def kprobe_add_raw_field(name, probe, fields=None):
    """
    Add a raw definition of a data field to the probe descriptor.

    Parameters
    ----------
    name : string
        The name of the kprobe event.
    probe : string
        Definition of the probe.
    fields : dictionary (string : string)
        Input location for a probe descriptor. If provided, the new field will be added to it.
        Otherwise new probe descriptor will be generated.

    Returns
    -------
    fields: dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    if fields is None:
        fields = {}

    fields[str(name)] = str(probe)
    return fields


def kprobe_add_arg(name, param_id, param_type, fields=None):
    """
    Add a function parameter data field to the probe descriptor.

    Parameters
    ----------
    name : string
        The name of the kprobe event.
    param_id : int
        The number of the function parameter to be recorded.
    param_type : string
        The type of the function parameter to be recorded. Following types are supported:
           * Basic types (u8/u16/u32/u64/s8/s16/s32/s64)
           * Hexadecimal types (x8/x16/x32/x64)
           * "string" and "ustring"
    fields : dictionary (string : string)
        Input location for a probe descriptor. If provided, the new field will be added to it.
        Otherwise new probe descriptor will be generated.

    Returns
    -------
    fields: dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    probe = '$arg{0}:{1}'.format(param_id, param_type)
    return kprobe_add_raw_field(name=name, probe=probe, fields=fields)


def kprobe_add_ptr_arg(name, param_id, param_type, offset=0, fields=None):
    """
    Add a pointer function parameter data field to the probe descriptor.

    Parameters
    ----------
    name : string
        The name of the kprobe event.
    param_id : int
        The number of the function parameter to be recorded.
    param_type : string
        The type of the function parameter to be recorded. Following types are supported:
           * Basic types (u8/u16/u32/u64/s8/s16/s32/s64)
           * Hexadecimal types (x8/x16/x32/x64)
           * "string" and "ustring"
    offset : int
        Memory offset of the function parameter's field.
    fields : dictionary (string : string)
        Input location for a probe descriptor. If provided, the new field will be added to it.
        Otherwise new probe descriptor will be generated.

    Returns
    -------
    fields: dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    probe = '+{0}($arg{1}):{2}'.format(offset, param_id, param_type)
    return kprobe_add_raw_field(name=name, probe=probe, fields=fields)


def kprobe_add_array_arg(name, param_id, param_type, offset=0,
                         size=-1, fields=None):
    """
    Add an array function parameter data field to the probe descriptor.

    Parameters
    ----------
    name : string
        The name of the kprobe event.
    param_id : int
        The number of the function parameter to be recorded.
    param_type : string
        The type of the function parameter to be recorded. Following types are supported:
           * Basic types (u8/u16/u32/u64/s8/s16/s32/s64)
           * Hexadecimal types (x8/x16/x32/x64)
           * "string" and "ustring"
    offset : int
        Memory offset of the function parameter's field.
    size : int
        The size of the array. If not provided, 10 array elements are recorded.
    fields : dictionary (string : string)
        Input location for a probe descriptor. If provided, the new field will be added to it.
        Otherwise new probe descriptor will be generated.

    Returns
    -------
    fields: dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    if size < 0:
        size = 10

    ptr_size = ctypes.sizeof(ctypes.c_voidp)
    for i in range(size):
        field_name = name + str(i)
        probe = '+{0}(+{1}'.format(offset, i * ptr_size)
        probe += '($arg{0})):{1}'.format(param_id, param_type)
        return kprobe_add_raw_field(name=field_name, probe=probe, fields=fields)


def kprobe_add_string_arg(name, param_id, offset=0, usr_space=False, fields=None):
    """
    Add a string function parameter data field to the probe descriptor.

    Parameters
    ----------
    name : string
        The name of the kprobe event.
    param_id : int
        The number of the function parameter to be recorded.
    offset : int
        Memory offset of the function parameter's field.
    usr_space : bool
        Is this a 'user space' or a 'kernel space' parameter.
    fields : dictionary (string : string)
        Input location for a probe descriptor. If provided, the new field will be added to it.
        Otherwise new probe descriptor will be generated.

    Returns
    -------
    fields: dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    p_type = 'ustring' if usr_space else 'string'
    return kprobe_add_ptr_arg(name=name,
                              param_id=param_id,
                              param_type=p_type,
                              offset=offset,
                              fields=fields)


def kprobe_add_string_array_arg(name, param_id, offset=0, usr_space=False,
                                size=-1, fields=None):
    """
    Add a string array function parameter data field to the probe descriptor.

    Parameters
    ----------
    name : string
        The name of the kprobe event.
    param_id : int
        The number of the function parameter to be recorded.
    offset : int
        Memory offset of the function parameter's field.
    usr_space : bool
        Is this a 'user space' or a 'kernel space' parameter.
    size : int
        The size of the array. If not provided, 10 array elements are recorded.
    fields : dictionary (string : string)
        Input location for a probe descriptor. If provided, the new field will be added to it.
        Otherwise new probe descriptor will be generated.

    Returns
    -------
    fields: dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    p_type = 'ustring' if usr_space else 'string'
    return kprobe_add_array_arg(name=name,
                                param_id=param_id,
                                param_type=p_type,
                                offset=offset,
                                size=size,
                                fields=fields)


def kprobe_parse_record_array_field(event, record, field, size=-1):
    """
    Parse the content of an array function parameter data field.

    Parameters
    ----------
    event : PyTepEvent
        The event descriptor.
    record : PyTepRecord
        The record.

    Returns
    -------
    fields : list of int or string
        The values of the array field.
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


class tc_kretval_probe(_kprobe_base):
    """
    A class used to represent a kretval probe.
    """
    def __init__(self, name, func):
        """
        Constructor.

        Parameters
        ----------
        name : string
            The name of the kprobe event.
        func: string
            Name of a function or address.
        """
        super().__init__(name, func)
        self.evt = ft.kprobe(event=self.name, function=self.func)
        self.register()


class tc_eprobe(_dynevent):
    """
    A class used to represent an event probe.

    Attributes
    ----------
    target_event : tc_event
        The event to attach the probe to.
    """
    def __init__(self, name, target_event, fields):
        """
        Constructor.

        Parameters
        ----------
        name : string
            The name of the kprobe event.
        target_event : tc_event
            The event to attach the probe to.
        fields : dictionary (string : string)
            Key - eprobe field name, value - definition of the probe.
        """
        super().__init__(name=name)
        self.target_event = target_event
        self.fields = fields
        probe = ' '.join('{!s}={!s}'.format(key,val) for (key, val) in self.fields.items())
        self.evt = ft.eprobe(event=self.name,
                            target_system=target_event.system,
                            target_event=target_event.name,
                            fetch_fields=probe)
        self.register()


def eprobe_add_ptr_field(name, target_field, field_type, offset=0, fields=None):
    """
    Add a pointer data field to the eprobe descriptor.

    Parameters
    ----------
    name : string
        The name of the kprobe event.
    param_id : int
        The number of the function parameter to be recorded.
    field_type : string
        The type of the event field to be recorded. Following types are supported:
           * Basic types (u8/u16/u32/u64/s8/s16/s32/s64)
           * Hexadecimal types (x8/x16/x32/x64)
           * "string" and "ustring"
    offset : int
        Memory offset of the function parameter's field.
    fields : dictionary (string : string)
        Input location for a probe descriptor. If provided, the new field will be added to it.
        Otherwise new probe descriptor will be generated.

    Returns
    -------
    fields: dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    probe = '+{0}(${1}):{2}'.format(offset, target_field, field_type)
    return kprobe_add_raw_field(name=name, probe=probe, fields=fields)


def eprobe_add_string_field(name, target_field, offset=0, usr_space=False, fields=None):
    """
    Add a string data field to the eprobe descriptor.

    Parameters
    ----------
    name : string
        The name of the kprobe event.
    param_id : int
        The number of the function parameter to be recorded.
    offset : int
        Memory offset of the function parameter's field.
    fields : dictionary (string : string)
        Input location for a probe descriptor. If provided, the new field will be added to it.
        Otherwise new probe descriptor will be generated.

    Returns
    -------
    fields: dictionary (string : string)
        Key - kprobe field name, value - definition of the probe.
    """
    f_type = 'ustring' if usr_space else 'string'
    return eprobe_add_ptr_field(name=name,
                                target_field=target_field,
                                field_type=f_type,
                                offset=offset,
                                fields=fields)


class tc_hist:
    """
    A class used to represent a kernel histogram.

    Attributes
    ----------
    name:
        The name of the histogram.
    inst : PyTfsInstance
        The unique Ftrace instance used by the histogram.
    attached : bool
        Is this object attached to the Python module.
    hist : PyTraceHist
        Low-level kernel histogram object.
    trigger : string
        Path to the trigger file of the histogram.
    """
    def __init__(self, name, event, axes, weights,
                 sort_keys, sort_dir, find=False):
        """
        Constructor.

        Parameters
        ----------
        name : string
            The name of the kernel histogram.
        event : tc_event
            The event to attach the histogram to.
        axes : dictionary (string : string or int) (optional)
            Definition of the histogram.
            Key - name of a field from the histogram's event.
            Value - the type of the probe. The following types are supported:
                'normal', 'n' or 0 for displaying a number;
                'hex', 'h' or 1 for displaying a number as a hex value;
                'sym' or 2 for displaying an address as a symbol;
                'sym_offset', 'so' or 3 for displaying an address as a symbol and offset;
                'syscall', 'sc' or 4 for displaying a syscall id as a system call name;
                'execname', 'e' or 5 for displaying a common_pid as a program name;
                'log', 'l' or 6 for displaying log2 value rather than raw number;
        weights : string
            Event field name, to be used as a weight of the individual histogram entries.
        sort_keys : string or list of strings
            The keys to sort.
        sort_dir : dictionary (string :int or string )
            Key - histogram's key to set sort direction, val - direction.
                Use 0, 'a', 'asc' or 'ascending' for sorting in ascending direction.
                Use 1, 'd', 'desc' or 'descending' for sorting in descending direction.
        find : bool
            Find an existing histogram or create a new one.
        """
        self.name = name
        self.inst = None

        inst_name = name + '_inst'
        if find:
            self.inst = ft.find_instance(name=inst_name)
            self.attached = False
        else:
            self.inst = ft.create_instance(name=inst_name)
            self.attached = True

        self.hist = ft.hist(name=name,
                            system=event.system,
                            event=event.name,
                            axes=axes)

        for v in weights:
            self.hist.add_value(value=v)

        self.hist.sort_keys(keys=sort_keys)

        for key, val in sort_dir.items():
            self.hist.sort_key_direction(sort_key=key,
                                         direction=val)

        self.trigger = '{0}/events/{1}/{2}/trigger'.format(self.inst.dir(),
                                                           event.system,
                                                           event.name)

        if not find:
            # Put the kernel histogram on 'standby'.
            self.hist.stop(self.inst)

    def __del__(self):
        """
        Destructor.
        """
        if self.inst and self.attached:
            self.clear()

    def start(self):
        """
        Start accumulating data.
        """
        self.hist.resume(self.inst)

    def stop(self):
        """
        Stop accumulating data.
        """
        self.hist.stop(self.inst)

    def resume(self):
        """
        Continue accumulating data.
        """
        self.hist.resume(self.inst)

    def data(self):
        """
        Read the accumulated data.
        """
        return self.hist.read(self.inst)

    def clear(self):
        """
        Clear the accumulated data.
        """
        self.hist.clear(self.inst)

    def detach(self):
        """
        Detach the object from the Python module.
        """
        ft.detach(self.inst)
        self.attached = False

    def attach(self):
        """
        Attach the object to the Python module.
        """
        ft.attach(self.inst)
        self.attached = True

    def is_attached(self):
        """
        Check if the object is attached to the Python module.
        """
        return self.attached

    def __repr__(self):
        """
        Read the descriptor of the histogram.
        """
        with open(self.trigger) as f:
            return f.read().rstrip()

    def __str__(self):
        """
        Read the accumulated data.
        """
        return self.data()


def create_hist(name, event, axes, weights=None, sort_keys=None, sort_dir=None):
    """
    Create new kernel histogram.

    Parameters
    ----------
    name : string
        The name of the kernel histogram.
    event : tc_event
        The event to attach the histogram to.
    axes : dictionary (string : string or int) (optional)
        Definition of the histogram.
        Key - name of a field from the histogram's event.
        Value - the type of the probe. The following types are supported:
            'normal', 'n' or 0 for displaying a number;
            'hex', 'h' or 1 for displaying a number as a hex value;
            'sym' or 2 for displaying an address as a symbol;
            'sym_offset', 'so' or 3 for displaying an address as a symbol and offset;
            'syscall', 'sc' or 4 for displaying a syscall id as a system call name;
            'execname', 'e' or 5 for displaying a common_pid as a program name;
            'log', 'l' or 6 for displaying log2 value rather than raw number;
    weights : string
        Event field name, to be used as a weight of the individual histogram entries.
    sort_keys : string or list of strings
        The keys to sort.
    sort_dir : dictionary (string :int or string )
        Key - histogram's key to set sort direction, val - direction.
            Use 0, 'a', 'asc' or 'ascending' for sorting in ascending direction.
            Use 1, 'd', 'desc' or 'descending' for sorting in descending direction.
    """
    if weights is None:
        weights = []

    if sort_keys is None:
        sort_keys = []

    if sort_dir is None:
        sort_dir = {}

    try:
        hist = tc_hist(name=name, event=event, axes=axes, weights=weights,
                       sort_keys=sort_keys, sort_dir=sort_dir, find=False)
    except Exception as err:
        msg = 'Failed to create histogram \'{0}\''.format(name)
        raise RuntimeError(msg) from err

    return hist


def find_hist(name, event, axes, weights=None, sort_keys=None, sort_dir=None):
    """
    Find existing kernel histogram.

    Parameters
    ----------
    name : string
        The name of the kernel histogram.
    event : tc_event
        The event to attach the histogram to.
    axes : dictionary (string : string or int) (optional)
        Definition of the histogram.
        Key - name of a field from the histogram's event.
        Value - the type of the probe. The following types are supported:
            'normal', 'n' or 0 for displaying a number;
            'hex', 'h' or 1 for displaying a number as a hex value;
            'sym' or 2 for displaying an address as a symbol;
            'sym_offset', 'so' or 3 for displaying an address as a symbol and offset;
            'syscall', 'sc' or 4 for displaying a syscall id as a system call name;
            'execname', 'e' or 5 for displaying a common_pid as a program name;
            'log', 'l' or 6 for displaying log2 value rather than raw number;
    weights : string
        Event field name, to be used as a weight of the individual histogram entries.
    sort_keys : string or list of strings
        The keys to sort.
    sort_dir : dictionary (string :int or string )
        Key - histogram's key to set sort direction, val - direction.
            Use 0, 'a', 'asc' or 'ascending' for sorting in ascending direction.
            Use 1, 'd', 'desc' or 'descending' for sorting in descending direction.
    """
    if weights is None:
        weights = []

    if sort_keys is None:
        sort_keys = []

    if sort_dir is None:
        sort_dir = {}

    try:
        hist = tc_hist(name=name, event=event, axes=axes, weights=weights,
                       sort_keys=sort_keys, sort_dir=sort_dir, find=True)
    except Exception as err:
        msg = 'Failed to find histogram \'{0}\''.format(name)
        raise RuntimeError(msg) from err

    return hist


class tc_synth(tc_event):
    """
    A class used to represent a kernel histogram.

    Attributes
    ----------
    synth:
        Low-level synthetic event object.
    """
    def __init__(self, name, start_event, end_event,
                 synth_fields=None, match_name=ft.no_arg()):
        """
        Constructor.

        Parameters
        ----------
        name : string
            The name of the synthetic event.
        start_event : dictionary
            Start event descriptor. To be generated using synth_event_item().
        end_event : dictionary
            End event descriptor. To be generated using synth_event_item().
        synth_fields : list of strings
            Synthetic field descriptors. Each of the descriptors be generated using:
                - synth_field_deltaT()
                - synth_field_delta_start()
                - synth_field_delta_end()
                - synth_field_sum()
        match_name : string
            If used, the match value will be recorded as a field, using the name provided.
        """
        super().__init__(system='synthetic', name=name, static=False)
        self.synth = ft.synth(name,
                              start_sys=start_event['event'].system,
                              start_evt=start_event['event'].name,
                              end_sys=end_event['event'].system,
                              end_evt=end_event['event'].name,
                              start_match=start_event['match'],
                              end_match=end_event['match'],
                              match_name=match_name)

        start_fields = ([], start_event['fields'])['fields' in start_event]
        start_field_names = ([None] * len(start_fields),
                             start_event['field_names'])['field_names' in start_event]

        self.synth.add_start_fields(fields=start_fields,
                                    names=start_field_names)

        end_fields = ([], end_event['fields'])['fields' in end_event]
        end_field_names = ([None] * len(end_fields),
                           end_event['field_names'])['field_names' in end_event]

        self.synth.add_end_fields(fields=end_fields,
                                  names=end_field_names)

        if synth_fields is not None:
            for f in synth_fields:
                args = f.split(' ')
                if args[0] == 'delta_t' and len(args) < 4:
                    # The expected format is: 'delta_t [name] hd'.
                    # Arguments 1 and 2 are optional.
                    if len(args) == 1:
                        self.synth.add_delta_T()
                    elif len(args) == 3 and args[2] == 'hd':
                        self.synth.add_delta_T(name=args[1], hd=True)
                    elif args[1] == 'hd':
                        self.synth.add_delta_T(hd=True)
                    else:
                        self.synth.add_delta_T(name=args[1])
                elif args[0] == 'delta_start' and len(args) == 4:
                    # The expected format is:
                    # 'delta_start [name] [start_field] [end_field]'.
                    self.synth.add_delta_start(name=args[1],
                                               start_field=args[2],
                                               end_field=args[3])

                elif args[0] == 'delta_end' and len(args) == 4:
                    # The expected format is:
                    # 'delta_end [name] [start_field] [end_field]'.
                    self.synth.add_delta_end(name=args[1],
                                             start_field=args[2],
                                             end_field=args[3])

                elif args[0] == 'sun' and len(args) == 4:
                    # The expected format is:
                    # 'sum [name] [start_field] [end_field]'.
                    self.synth.add_sum(name=args[1],
                                       start_field=args[2],
                                       end_field=args[3])

                else:
                    raise ValueError('Invalid synth. field \'{0}\''.format(f))

        self.synth.register()
        self.evt_id = find_event_id(system=self.system, event=self.name)

    def __repr__(self):
        """
        Read the descriptor of the synthetic event.
        """
        return self.synth.repr(event=True, hist_start=True, hist_end=True)


def synth_event_item(event, match, fields=None):
    """
    Create descriptor for an event item (component) of a synthetic event.
    To be used as a start/end event.

    Parameters
    ----------
    event : tc_event
        An event to attach the synthetic event to.
    match : string
        Field from the event to be used for matching.
    fields : list of strings
        Fields from the event to be recorded.
    """
    if fields is None:
        fields = []

    sub_evt = {}
    sub_evt['event'] = event
    sub_evt['fields'] = fields
    sub_evt['field_names'] = [None] * len(fields)
    sub_evt['match'] = match

    return sub_evt


def synth_field_rename(event, field, name):
    """
    Change the name of a field in the event component of a synthetic event.Parameters
    ----------
    event : dictionary
        Event descriptor, generated using synth_event_item().
    field : string
        The original name of the field to be renamed.
    name : string
        New name for the field.
    """
    pos = event['fields'].index(field)
    event['field_names'][pos] = name

    return event


def synth_field_deltaT(name='delta_T', hd=False):
    """
    Create descriptor for time-diference (end - start) synthetic field.
    The default time resolution is in microseconds.

    Parameters
    ----------
    name : string
        The name of the new synthetic field.
    hd : bool
        If True, use 'hd' time resolution (nanoseconds).
    """
    d = 'delta_t {0}'.format(name)
    return (d, d+' hd')[hd]


def synth_field_delta_start(name, start_field, end_field):
    """
    Create descriptor for field diference (start - end) synthetic field.

    Parameters
    ----------
    name : string
        The name of the new synthetic field.
    start_field : string
        A field from the start event.
    end_field : string
        A field from the end event.
    """
    return 'delta_start {0} {1} {2}'.format(name, start_field, end_field)


def synth_field_delta_end(name, start_field, end_field):
    """
    Create descriptor for field diference (end - start)  synthetic field.

    Parameters
    ----------
    name : string
        The name of the new synthetic field.
    start_field : string
        A field from the start event.
    end_field : string
        A field from the end event
    """
    return 'delta_end {0} {1} {2}'.format(name, start_field, end_field)


def synth_field_sum(name, start_field, end_field):
    """
    Create descriptor for field sum (start + end) synthetic field.

    Parameters
    ----------
    name : string
        The name of the new synthetic field.
    start_field : string
        A field from the start event.
    end_field : string
        A field from the end event
    """
    return 'sum {0} {1} {2}'.format(name, start_field, end_field)

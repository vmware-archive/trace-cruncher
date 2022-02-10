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
    tep = ft.tep_handle()
    tep.init_local(dir=ft.dir())

    return tep


def find_event_id(system, event):
    """ Get the unique identifier of a trace event.
    """
    tep = ft.tep_handle()
    tep.init_local(dir=ft.dir(), systems=[system])

    return tep.get_event(system=system, name=event).id()


def short_kprobe_print(tep, events):
    """ Register short (no probe address) print for these kprobe events.
    """
    for e in events:
        if len(e.fields):
            tep.short_kprobe_print(id=e.evt_id, system=e.system, event=e.name)


class tc_event:
    def __init__(self, system, name, static=True):
        """ Constructor.
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
        """ Retrieve the unique ID of the event.
        """
        return int(self.evt_id)

    def enable(self, instance=ft.no_arg()):
        """ Enable this event.
        """
        ft.enable_event(instance=instance, system=self.system, event=self.name)

    def disable(self, instance=ft.no_arg()):
        """ Disable this event.
        """
        ft.disable_event(instance=instance,system=self.system, event=self.name)

    def set_filter(self, filter, instance=ft.no_arg()):
        """ Define a filter for this event.
        """
        ft.set_event_filter(instance=instance,
                            system=self.system,
                            event=self.name,
                            filter=filter)

    def clear_filter(self, instance=ft.no_arg()):
        """ Clear the filter for this event.
        """
        ft.clear_event_filter(instance=instance,
                              system=self.system,
                              event=self.name)


class _dynevent(tc_event):
    def __init__(self, name):
        """ Constructor.
        """
        super().__init__(system=ft.tc_event_system(), name=name, static=False)
        self.evt = None
        self.evt_id = -1

    def register(self):
        """ Register this probe to Ftrace.
        """
        self.evt.register()
        self.evt_id = find_event_id(system=ft.tc_event_system(), event=self.name)


class _kprobe_base(_dynevent):
    def __init__(self, name, func):
        """ Constructor.
        """
        super().__init__(name=name)
        self.func = func


class tc_kprobe(_kprobe_base):
    def __init__(self, name, func, fields):
        """ Constructor.
        """
        super().__init__(name, func)
        self.fields = fields
        probe = ' '.join('{!s}={!s}'.format(key,val) for (key, val) in self.fields.items())
        self.evt = ft.kprobe(event=self.name, function=self.func, probe=probe)
        self.register()


def kprobe_add_raw_field(name, probe, fields={}):
    """ Add a raw definition of a data field to the probe descriptor.
    """
    fields[str(name)] = str(probe)
    return fields


def kprobe_add_arg(name, param_id, param_type, fields={}):
    """ Add a function parameter data field to the probe descriptor.
    """
    probe = '$arg{0}:{1}'.format(param_id, param_type)
    return kprobe_add_raw_field(name=name, probe=probe, fields=fields)


def kprobe_add_ptr_arg(name, param_id, param_type, offset=0, fields={}):
    """ Add a pointer function parameter data field to the probe descriptor.
    """
    probe = '+{0}($arg{1}):{2}'.format(offset, param_id, param_type)
    return kprobe_add_raw_field(name=name, probe=probe, fields=fields)


def kprobe_add_array_arg(name, param_id, param_type, offset=0,
                         size=-1, fields={}):
    """ Add an array function parameter data field to the probe descriptor.
    """
    if size < 0:
        size = 10

    ptr_size = ctypes.sizeof(ctypes.c_voidp)
    for i in range(size):
        field_name = name + str(i)
        probe = '+{0}(+{1}'.format(offset, i * ptr_size)
        probe += '($arg{0})):{1}'.format(param_id, param_type)
        return kprobe_add_raw_field(name=field_name, probe=probe, fields=fields)


def kprobe_add_string_arg(name, param_id, offset=0, usr_space=False, fields={}):
    """ Add a string function parameter data field to the probe descriptor.
    """
    p_type = 'ustring' if usr_space else 'string'
    return kprobe_add_ptr_arg(name=name,
                              param_id=param_id,
                              param_type=p_type,
                              offset=offset,
                              fields=fields)


def kprobe_add_string_array_arg(name, param_id, offset=0, usr_space=False,
                                size=-1, fields={}):
    """ Add a string array function parameter data field to the probe descriptor.
    """
    p_type = 'ustring' if usr_space else 'string'
    return kprobe_add_array_arg(name=name,
                                param_id=param_id,
                                param_type=p_type,
                                offset=offset,
                                size=size,
                                fields=fields)


def kprobe_parse_record_array_field(event, record, field, size=-1):
    """ Parse the content of an array function parameter data field.
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
    def __init__(self, name, func):
        """ Constructor.
        """
        super().__init__(name, func)
        self.evt = ft.kprobe(event=self.name, function=self.func)
        self.register()


class tc_eprobe(_dynevent):
    def __init__(self, name, target_event, fields):
        """ Constructor.
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


def eprobe_add_ptr_field(name, target_field, field_type, offset=0, fields={}):
    """ Add a pointer data field to the eprobe descriptor.
    """
    probe = '+{0}(${1}):{2}'.format(offset, target_field, field_type)
    return kprobe_add_raw_field(name=name, probe=probe, fields=fields)


def eprobe_add_string_field(name, target_field, offset=0, usr_space=False, fields={}):
    """ Add a string data field to the eprobe descriptor.
    """
    f_type = 'ustring' if usr_space else 'string'
    return eprobe_add_ptr_field(name=name,
                                target_field=target_field,
                                field_type=f_type,
                                offset=offset,
                                fields=fields)


class tc_hist:
    def __init__(self, name, event, axes, weights=[],
                 sort_keys=[], sort_dir={}, find=False):
        """ Constructor.
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
            # Put the kernel histogram on 'standby'
            self.hist.stop(self.inst)

    def __del__(self):
        """ Destructor.
        """
        if self.inst and self.attached:
            self.clear()

    def start(self):
        """ Start accumulating data.
        """
        self.hist.resume(self.inst)

    def stop(self):
        """ Stop accumulating data.
        """
        self.hist.stop(self.inst)

    def resume(self):
        """ Continue accumulating data.
        """
        self.hist.resume(self.inst)

    def data(self):
        """ Read the accumulated data.
        """
        return self.hist.read(self.inst)

    def clear(self):
        """ Clear the accumulated data.
        """
        self.hist.clear(self.inst)

    def detach(self):
        """ Detach the object from the Python module.
        """
        ft.detach(self.inst)
        self.attached = False

    def attach(self):
        """ Attach the object to the Python module.
        """
        ft.attach(self.inst)
        self.attached = True

    def is_attached(self):
        """ Check if the object is attached to the Python module.
        """
        return self.attached

    def __repr__(self):
        """ Read the descriptor of the histogram.
        """
        with open(self.trigger) as f:
            return f.read().rstrip()

    def __str__(self):
        return self.data()


def create_hist(name, event, axes, weights=[], sort_keys=[], sort_dir={}):
    """ Create new kernel histogram.
    """
    try:
        hist = tc_hist(name=name, event=event, axes=axes, weights=weights,
                       sort_keys=sort_keys, sort_dir=sort_dir, find=False)
    except Exception as err:
        msg = 'Failed to create histogram \'{0}\''.format(name)
        raise RuntimeError(msg) from err

    return hist


def find_hist(name, event, axes, weights=[], sort_keys=[], sort_dir={}):
    """ Find existing kernel histogram.
    """
    try:
        hist = tc_hist(name=name, event=event, axes=axes, weights=weights,
                       sort_keys=sort_keys, sort_dir=sort_dir, find=True)
    except Exception as err:
        msg = 'Failed to find histogram \'{0}\''.format(name)
        raise RuntimeError(msg) from err

    return hist


class tc_synth(tc_event):
    def __init__(self, name, start_event, end_event,
                 synth_fields=None, match_name=ft.no_arg()):
        """ Constructor.
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
        """ Read the descriptor of the synthetic event.
        """
        return self.synth.repr(event=True, hist_start=True, hist_end=True)


def synth_event_item(event, match, fields=[]):
    """ Create descriptor for an event item (component) of a synthetic event.
        To be used as a start/end event.
    """
    sub_evt = {}
    sub_evt['event'] = event
    sub_evt['fields'] = fields
    sub_evt['field_names'] = [None] * len(fields)
    sub_evt['match'] = match

    return sub_evt


def synth_field_rename(event, field, name):
    """ Change the name of a field in the event component of a synthetic event.
    """
    pos = event['fields'].index(field)
    event['field_names'][pos] = name

    return event


def synth_field_deltaT(name='delta_T', hd=False):
    """ Create descriptor for time-diference synthetic field.
    """
    d = 'delta_t {0}'.format(name)
    return (d, d+' hd')[hd]


def synth_field_delta_start(name, start_field, end_field):
    """ Create descriptor for field diference (start - end) synthetic field.
    """
    return 'delta_start {0} {1} {2}'.format(name, start_field, end_field)


def synth_field_delta_end(name, start_field, end_field):
    """ Create descriptor for field diference (end - start)  synthetic field.
    """
    return 'delta_end {0} {1} {2}'.format(name, start_field, end_field)


def synth_field_sum(name, start_field, end_field):
    """ Create descriptor for field sum synthetic field.
    """
    return 'sum {0} {1} {2}'.format(name, start_field, end_field)

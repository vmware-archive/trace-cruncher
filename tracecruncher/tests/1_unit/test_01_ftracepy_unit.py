#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import os
import sys
import unittest
import tracecruncher.ftracepy as ft
import tracecruncher.ft_utils as tc

instance_name = 'test_instance1'
another_instance_name = 'test_instance2'
kernel_version = tuple(map(int, (os.uname()[2].split('.')[:2])))

class InstanceTestCase(unittest.TestCase):
    def test_dir(self):
        tracefs_dir = ft.dir()
        self.assertTrue(os.path.isdir(tracefs_dir))
        instances_dir = tracefs_dir + '/instances/'
        self.assertTrue(os.path.isdir(instances_dir))

    def test_create_instance(self):
        inst = ft.create_instance(instance_name)
        self.assertTrue(ft.is_tracing_ON(inst))
        instances_dir = ft.dir() + '/instances/'
        self.assertTrue(os.path.isdir(instances_dir + instance_name))

        auto_inst = ft.create_instance(tracing_on=False)
        self.assertFalse(ft.is_tracing_ON(auto_inst))

    def test_instance_dir(self):
        inst = ft.create_instance(instance_name)
        tracefs_dir = ft.dir();
        instance_dir = tracefs_dir + '/instances/' + instance_name
        self.assertEqual(instance_dir, inst.dir())

    def test_find(self):
        inst = ft.create_instance(instance_name)
        tracefs_dir = ft.dir();
        instance_dir = tracefs_dir + '/instances/' + instance_name
        inst_1 = ft.find_instance(instance_name)
        self.assertEqual(instance_dir, inst_1.dir())

        err='Failed to find trace instance'
        with self.assertRaises(Exception) as context:
            inst_2 = ft.find_instance(another_instance_name)
        self.assertTrue(err in str(context.exception))

    def test_1_detach(self):
        inst = ft.create_instance(instance_name)
        ft.detach(inst)

    def test_2_attach(self):
        inst = ft.find_instance(instance_name)
        ft.attach(inst)

    def test_3_attach(self):
        tracefs_dir = ft.dir();
        instance_dir = tracefs_dir + '/instances/' + instance_name
        self.assertFalse(os.path.isdir(instance_dir))


class PyTepTestCase(unittest.TestCase):
    def test_init_local(self):
        tracefs_dir = ft.dir()
        tep = ft.tep_handle();
        tep.init_local(tracefs_dir);

        tep.init_local(dir=tracefs_dir, systems=['sched', 'irq']);

        inst = ft.create_instance(instance_name)
        tracefs_dir = inst.dir()
        tep.init_local(dir=tracefs_dir, systems=['sched', 'irq']);

        err='function missing required argument \'dir\''
        with self.assertRaises(Exception) as context:
            tep.init_local(systems=['sched', 'irq']);
        self.assertTrue(err in str(context.exception))

        err='Failed to get local \'tep\' event from /no/dir'
        with self.assertRaises(Exception) as context:
            tep.init_local(dir='/no/dir', systems=['sched', 'irq'])
        self.assertTrue(err in str(context.exception))

    def test_get_event(self):
        tracefs_dir = ft.dir()
        tep = ft.tep_handle();
        tep.init_local(tracefs_dir);
        evt = tep.get_event(system='sched', name='sched_switch');


class PyTepEventTestCase(unittest.TestCase):
    def test_name(self):
        tracefs_dir = ft.dir()
        tep = ft.tep_handle();
        tep.init_local(tracefs_dir);
        evt = tep.get_event(system='sched', name='sched_switch');
        self.assertEqual(evt.name(), 'sched_switch');

    def test_field_names(self):
        tracefs_dir = ft.dir()
        tep = ft.tep_handle();
        tep.init_local(tracefs_dir);
        evt = tep.get_event(system='sched', name='sched_switch');
        fiels = evt.field_names()
        self.assertEqual(fiels , ['common_type',
                                  'common_flags',
                                  'common_preempt_count',
                                  'common_pid',
                                  'prev_comm',
                                  'prev_pid',
                                  'prev_prio',
                                  'prev_state',
                                  'next_comm',
                                  'next_pid',
                                  'next_prio'])


class TracersTestCase(unittest.TestCase):
    def test_available_tracers(self):
        tracers = ft.available_tracers()
        self.assertTrue(len(tracers) > 1)
        self.assertTrue('function' in tracers)

    def test_set_tracer(self):
        ft.set_current_tracer(tracer='function')
        ft.set_current_tracer(tracer='')

        err = 'Tracer \'zero\' is not available.'
        with self.assertRaises(Exception) as context:
            ft.set_current_tracer(tracer='zero')
        self.assertTrue(err in str(context.exception))


class EventsTestCase(unittest.TestCase):
    def test_available_systems(self):
        systems = ft.available_event_systems(sort=True)
        self.assertTrue(len(systems) > 2)
        self.assertTrue('sched' in systems)
        for i in range(len(systems) - 2):
            self.assertTrue(systems[i] < systems[i + 1])

        inst = ft.create_instance(instance_name)
        systems = ft.available_event_systems(inst)
        self.assertTrue(len(systems) > 2)
        self.assertTrue('sched' in systems)

    def test_available_system_events(self):
        events = ft.available_system_events(system='sched', sort=True)
        self.assertTrue(len(events) > 2)
        self.assertTrue('sched_switch' in events)
        for i in range(len(events) - 2):
            self.assertTrue(events[i] < events[i + 1])

        inst = ft.create_instance(instance_name)
        events = ft.available_system_events(instance=inst,
                                            system='sched')
        self.assertTrue(len(events) > 1)
        self.assertTrue('sched_switch' in events)

        err = 'function missing required argument'
        with self.assertRaises(Exception) as context:
            ft.available_system_events(instance=inst)
        self.assertTrue(err in str(context.exception))

    def test_enable_event(self):
        inst = ft.create_instance(instance_name)
        ret = ft.event_is_enabled(instance=inst, system='all')
        self.assertEqual(ret, '0')
        ft.enable_event(instance=inst, system='all')
        ret = ft.event_is_enabled(instance=inst, system='all')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=inst, system='all')
        ret = ft.event_is_enabled(instance=inst, system='all')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=inst, event='all')
        self.assertEqual(ret, '0')
        ft.enable_event(instance=inst, event='all')
        ret = ft.event_is_enabled(instance=inst, event='all')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=inst, event='all')
        ret = ft.event_is_enabled(instance=inst, event='all')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=inst, system='sched')
        self.assertEqual(ret, '0')
        ft.enable_event(instance=inst, system='sched')
        ret = ft.event_is_enabled(instance=inst, system='sched')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=inst, system='sched')
        ret = ft.event_is_enabled(instance=inst, system='sched')
        self.assertEqual(ret, '0')

        ft.enable_event(instance=inst,
                        system='sched',
                        event='sched_switch')
        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=inst,
                         system='sched',
                         event='sched_switch')
        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '0')

        ft.enable_event(instance=inst,
                        system='sched',
                        event='all')
        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=inst,
                         system='sched',
                         event='all')
        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '0')

        ft.enable_event(instance=inst,
                        event='sched_switch')
        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=inst,
                         event='sched_switch')
        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '0')

        ft.enable_event(instance=inst,
                        system='all',
                        event='sched_switch')
        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=inst,
                         system='all',
                         event='sched_switch')
        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '0')

    def test_enable_event_err(self):
        inst = ft.create_instance(instance_name)

        err = 'Failed to enable/disable event'
        with self.assertRaises(Exception) as context:
            ft.enable_event(instance=inst,
                            system='zero')
        self.assertTrue(err in str(context.exception))

        with self.assertRaises(Exception) as context:
            ft.enable_event(instance=inst,
                            system='sched',
                            event='zero')
        self.assertTrue(err in str(context.exception))

    def test_enable_events(self):
        inst = ft.create_instance(instance_name)
        ft.enable_events(instance=inst,
                         events={'sched': ['all'],
                                 'irq': ['all']})

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=inst,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '1')

        ft.disable_events(instance=inst,
                          events={'sched': ['all'],
                                  'irq': ['all']})

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=inst,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '0')

        ft.enable_events(instance=inst,
                         events={'sched': ['sched_switch', 'sched_waking'],
                                 'irq':   ['all']})

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_waking')
        self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_wakeup')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=inst,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '1')

        ft.disable_events(instance=inst,
                          events={'sched': ['sched_switch', 'sched_waking'],
                                  'irq':   ['all']})

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='sched_waking')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=inst,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '0')

    def test_enable_events_err(self):
        inst = ft.create_instance(instance_name)

        err = 'Inconsistent \"events\" argument'
        with self.assertRaises(Exception) as context:
            ft.enable_events(instance=inst,
                             events='all')
        self.assertTrue(err in str(context.exception))

        err = 'Failed to enable/disable event'
        with self.assertRaises(Exception) as context:
            ft.enable_events(instance=inst,
                             events={'sched': ['no_event']})
        self.assertTrue(err in str(context.exception))


class OptionsTestCase(unittest.TestCase):
    def test_enable_option(self):
        inst = ft.create_instance(instance_name)
        opt = 'event-fork'
        ret = ft.option_is_set(instance=inst,
                               option=opt)
        self.assertFalse(ret)

        ft.enable_option(instance=inst,
                         option=opt)
        ret = ft.option_is_set(instance=inst,
                               option=opt)
        self.assertTrue(ret)

        ft.disable_option(instance=inst,
                          option=opt)
        ret = ft.option_is_set(instance=inst,
                                   option=opt)
        self.assertFalse(ret)

        opt = 'no-opt'
        err = 'Failed to set option \"no-opt\"'
        with self.assertRaises(Exception) as context:
            ft.enable_option(instance=inst,
                             option=opt)
        self.assertTrue(err in str(context.exception))

    def test_supported_options(self):
        inst = ft.create_instance(instance_name)
        opts = ft.supported_options(inst)
        self.assertTrue(len(opts) > 20)
        self.assertTrue('event-fork' in opts)

    def test_enabled_options(self):
        inst = ft.create_instance(instance_name)
        opts = ft.enabled_options(inst)
        n = len(opts)
        ft.enable_option(instance=inst, option='function-fork')
        ft.enable_option(instance=inst, option='event-fork')
        opts = ft.enabled_options(inst)

        self.assertEqual(len(opts), n + 2)
        self.assertTrue('event-fork' in opts)
        self.assertTrue('function-fork' in opts)


class KprobeTestCase(unittest.TestCase):
    def test_kprobe(self):
        evt1 = 'mkdir'
        evt1_func = 'do_mkdirat'
        evt1_prove = 'path=+u0($arg2):ustring'
        evt2 = 'open'
        evt2_func = 'do_sys_openat2'
        evt2_prove = 'file=+u0($arg2):ustring'

        kp1 = ft.kprobe(event=evt1, function=evt1_func, probe=evt1_prove)
        kp1.register()
        self.assertEqual(evt1, kp1.event())
        self.assertEqual(evt1_func, kp1.address())
        self.assertEqual(evt1_prove, kp1.probe())

        kp2 = ft.kprobe(event=evt2, function=evt2_func, probe=evt2_prove)
        kp2.register()
        self.assertEqual(evt2, kp2.event())
        self.assertEqual(evt2_func, kp2.address())
        self.assertEqual(evt2_prove, kp2.probe())

    def test_filter_kprobe(self):
        evt1 = 'mkdir'
        evt1_func = 'do_mkdirat'
        evt1_prove = 'path=+u0($arg2):ustring'
        flt = 'path~\'/sys/fs/cgroup/*\''

        kp1 = ft.kprobe(event=evt1, function=evt1_func, probe=evt1_prove)
        kp1.register()
        inst = ft.create_instance(instance_name)

        kp1.set_filter(instance=inst, filter=flt)
        flt_get =  kp1.get_filter(instance=inst)
        self.assertEqual(flt, flt_get)
        kp1.clear_filter(instance=inst)
        flt_get =  kp1.get_filter(instance=inst)
        self.assertEqual(flt_get, 'none')

    def test_enable_kprobe(self):
        evt1 = 'mkdir'
        evt1_func = 'do_mkdirat'
        evt1_prove = 'path=+u0($arg2):ustring'

        kp1 = ft.kprobe(event=evt1, function=evt1_func, probe=evt1_prove)
        kp1.register()
        inst = ft.create_instance(instance_name)
        kp1.enable(instance=inst)
        ret = kp1.is_enabled(instance=inst)
        self.assertEqual(ret, '1')

        kp1.disable(instance=inst)
        ret = kp1.is_enabled(instance=inst)
        self.assertEqual(ret, '0')

class EprobeTestCase(unittest.TestCase):
    def test_eprobe(self):
        """ Event probes are introduced in Linux kernel 5.15
        """

        evt1 = 'sopen_in'
        evt1_tsys = 'syscalls'
        evt1_tevent = 'sys_enter_openat'
        evt1_args = 'file=+0($filename):ustring'
        evt2 = 'sopen_out'
        evt2_tsys = 'syscalls'
        evt2_tevent = 'sys_exit_openat'
        evt2_args = 'res=$ret:u64'

        ep1 = ft.eprobe(event=evt1, target_system=evt1_tsys, target_event=evt1_tevent,
                        fetch_fields=evt1_args)
        self.assertEqual(evt1, ep1.event())
        self.assertEqual("{}.{}".format(evt1_tsys, evt1_tevent), ep1.address())
        self.assertEqual(evt1_args, ep1.probe())

        ep2 = ft.eprobe(event=evt2, target_system=evt2_tsys, target_event=evt2_tevent,
                        fetch_fields=evt2_args)
        self.assertEqual(evt2, ep2.event())
        self.assertEqual("{}.{}".format(evt2_tsys, evt2_tevent), ep2.address())
        self.assertEqual(evt2_args, ep2.probe())

    def test_enable_eprobe(self):
        """ Event probes are introduced in Linux kernel 5.15
        """
        if kernel_version < (5, 15):
            return

        evt1 = 'sopen_out'
        evt1_tsys = 'syscalls'
        evt1_tevent = 'sys_exit_openat'
        evt1_args = 'res=$ret:u64'

        ep1 = ft.eprobe(event=evt1, target_system=evt1_tsys, target_event=evt1_tevent,
                        fetch_fields=evt1_args)
        ep1.register()
        inst = ft.create_instance(instance_name)
        ep1.enable(instance=inst)
        ret = ep1.is_enabled(instance=inst)
        self.assertEqual(ret, '1')

        ep1.disable(instance=inst)
        ret = ep1.is_enabled(instance=inst)
        self.assertEqual(ret, '0')

class EprobeOopTestCase(unittest.TestCase):
    def test_eprobe(self):
        fields = tc.eprobe_add_string_field(name='file',
                                            target_field='filename',
                                            usr_space=True)
        self.assertEqual(fields, {'file': '+0($filename):ustring'})

        if kernel_version < (5, 15):
            return

        event = tc.tc_event('syscalls', 'sys_enter_openat')
        eprobe = tc.tc_eprobe(name='opn', target_event=event, fields=fields)
        self.assertEqual(eprobe.evt.probe(), 'file=+0($filename):ustring')

class TracingOnTestCase(unittest.TestCase):
    def test_ON_OF(self):
        ft.tracing_ON()
        self.assertTrue(ft.is_tracing_ON())
        ft.tracing_OFF()

        inst = ft.create_instance(instance_name)
        ft.tracing_ON(instance=inst)
        self.assertTrue(ft.is_tracing_ON(instance=inst))
        self.assertFalse(ft.is_tracing_ON())
        ft.tracing_OFF(instance=inst)

    def test_err(self):
        err = 'incompatible type'
        with self.assertRaises(Exception) as context:
            ft.tracing_ON('zero')
        self.assertTrue(err in str(context.exception))

        with self.assertRaises(Exception) as context:
            ft.tracing_OFF('zero')
        self.assertTrue(err in str(context.exception))


class HistTestCase(unittest.TestCase):
    def test_hist_create(self):
        inst = ft.create_instance(instance_name)
        sys = 'kmem'
        evt = 'kmalloc'
        tgr_file = inst.dir() + '/events/' +  sys + '/' + evt + '/trigger'

        f = open(tgr_file)

        hist = ft.hist(system=sys, event=evt, key='call_site', type='sym')
        hist.stop(inst)
        h_buff = f.read()
        self.assertTrue('hist:keys=call_site.sym' in h_buff)
        hist.close(inst)

        hist = ft.hist(system=sys, event=evt,
                       axes={'call_site': 'sym',
                             'bytes_alloc': 'n'})
        hist.stop(inst)
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('hist:keys=call_site.sym,bytes_alloc' in h_buff)
        hist.close(inst)

        hist = ft.hist(name='h2d', system=sys, event=evt,
                       axes={'bytes_req': 'log',
                             'bytes_alloc': 'h'})
        hist.stop(inst)
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('hist:name=h2d:keys=bytes_req.log2,bytes_alloc.hex' in h_buff)

        hist.close(inst)
        f.close()

    def test_hist_setup(self):
        inst = ft.create_instance(instance_name)
        sys = 'kmem'
        evt = 'kmalloc'
        tgr_file = inst.dir() + '/events/' +  sys + '/' + evt + '/trigger'

        f = open(tgr_file)

        hist = ft.hist(system=sys, event=evt,
                       axes={'call_site': 'sym',
                             'bytes_alloc': 'n'})

        hist.add_value(value='bytes_req')
        hist.stop(inst)
        h_buff = f.read()
        self.assertTrue(':vals=hitcount,bytes_req' in h_buff)

        hist.sort_keys(keys=['bytes_req', 'bytes_alloc'])
        hist.stop(inst)
        f.seek(0)
        h_buff = f.read()
        self.assertTrue(':sort=bytes_req,bytes_alloc' in h_buff)

        hist.sort_key_direction('bytes_req', 'desc')
        hist.stop(inst)
        f.seek(0)
        h_buff = f.read()
        self.assertTrue(':sort=bytes_req.descending,bytes_alloc' in str(h_buff))

        hist.close(inst)
        f.close()

    def test_hist_ctrl(self):
        inst = ft.create_instance(instance_name)
        sys = 'kmem'
        evt = 'kmalloc'
        tgr_file = inst.dir() + '/events/' +  sys + '/' + evt + '/trigger'

        f = open(tgr_file)

        hist = ft.hist(system=sys, event=evt,
                       axes={'call_site': 'sym',
                             'bytes_alloc': 'n'})

        hist.start(inst)
        h_buff = f.read()
        self.assertTrue('[active]' in h_buff)

        hist.stop(inst)
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('[paused]' in h_buff)

        hist.resume(inst)
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('[active]' in h_buff)

        h_buff = hist.read(inst)
        self.assertTrue('Totals:' in h_buff)

        hist.close(inst)
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('Available triggers:' in h_buff)
        f.close()

    def test_hist_err(self):
        inst = ft.create_instance(instance_name)
        hist = ft.hist(system='kmem', event='kmalloc',
                       axes={'call_site': 'sym',
                             'bytes_alloc': 'n'})
        err = 'Failed read data from histogram'
        with self.assertRaises(Exception) as context:
            hist.read(inst)
        self.assertTrue(err in str(context.exception))

        hist.start(inst)
        err = 'Failed to start filling the histogram'
        with self.assertRaises(Exception) as context:
            hist.start(inst)
        self.assertTrue(err in str(context.exception))
        hist.close(inst)


hist_name = 'h2d'
evt_sys = 'kmem'
evt_name = 'kmalloc'

axes={'call_site': 'sym',
      'bytes_req': 'n'}
weights=['bytes_alloc']
sort_keys=['bytes_req', 'bytes_alloc']
sort_dir={'bytes_req': 'desc'}

tgr_file = '{0}/instances/{1}_inst/events/{2}/{3}/trigger'.format(ft.dir(), hist_name, evt_sys, evt_name)

class HistOopTestCase(unittest.TestCase):
    def test_hist_create(self):
        evt = tc.tc_event(evt_sys, evt_name)
        hist = tc.create_hist(name=hist_name, event=evt, axes=axes, weights=weights,
                               sort_keys=sort_keys, sort_dir=sort_dir)
        f = open(tgr_file)
        h_buff = f.read()
        self.assertTrue('hist:name=h2d' in h_buff)
        self.assertTrue(':keys=call_site.sym,bytes_req' in h_buff)
        self.assertTrue(':vals=hitcount,bytes_alloc' in h_buff)
        self.assertTrue(':sort=bytes_req.descending,bytes_alloc' in str(h_buff))

        f.close()

    def test_hist_ctrl(self):
        evt = tc.tc_event(evt_sys, evt_name)
        hist = tc.create_hist(name=hist_name, event=evt, axes=axes, weights=weights,
                               sort_keys=sort_keys, sort_dir=sort_dir)
        f = open(tgr_file)
        h_buff = f.read()
        self.assertTrue('[paused]' in h_buff)

        hist.start()
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('[active]' in h_buff)

        hist.stop()
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('[paused]' in h_buff)

        hist.resume()
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('[active]' in h_buff)

        h_buff = hist.data()
        self.assertTrue('Totals:' in h_buff)

        hist.stop()
        hist.clear()
        f.seek(0)
        h_buff = f.read()
        self.assertTrue('[paused]' in h_buff)
        h_buff = hist.data()
        self.assertTrue('Hits: 0' in h_buff)
        self.assertTrue('Entries: 0' in h_buff)
        self.assertTrue('Dropped: 0' in h_buff)

        f.close()

    def test_1_detach(self):
        evt = tc.tc_event(evt_sys, evt_name)
        hist = tc.create_hist(name=hist_name, event=evt, axes=axes, weights=weights,
                              sort_keys=sort_keys, sort_dir=sort_dir)
        self.assertTrue(hist.is_attached())
        hist.detach()
        self.assertFalse(hist.is_attached())

    def test_2_attach(self):
        evt = tc.tc_event(evt_sys, evt_name)
        hist = tc.find_hist(name=hist_name, event=evt, axes=axes, weights=weights,
                            sort_keys=sort_keys, sort_dir=sort_dir)
        self.assertFalse(hist.is_attached())
        hist.attach()
        self.assertTrue(hist.is_attached())

    def test_hist_err(self):
        evt = tc.tc_event(evt_sys, evt_name)

        err = 'Failed to find histogram'
        with self.assertRaises(Exception) as context:
            hist = tc.find_hist(name=hist_name, event=evt, axes=axes, weights=weights,
                                sort_keys=sort_keys, sort_dir=sort_dir)
        self.assertTrue(err in str(context.exception))


class SynthTestCase(unittest.TestCase):
    def test_synt_create(self):
        synth = ft.synth(name='wakeup_lat',
                         start_sys='sched', start_evt='sched_waking',
                         end_sys='sched',   end_evt='sched_switch',
                         start_match='pid', end_match='next_pid',
                         match_name='pid')

        synth.add_start_fields(fields=['target_cpu', 'prio'],
                               names=['cpu', None])
        synth.add_end_fields(fields=['prev_prio', 'next_prio'],
                             names=[None, 'nxt_p'])
        synth.register()

        event = synth.repr(event=True, hist_start=False, hist_end=False)
        hist_s = synth.repr(event=False, hist_start=True, hist_end=False)
        hist_e = synth.repr(event=False, hist_start=False, hist_end=True)

        self.assertTrue('keys=pid'in hist_s)
        self.assertTrue('keys=next_pid' in hist_e)
        self.assertTrue('pid=next_pid' in hist_e)
        self.assertTrue('onmatch(sched.sched_waking).trace(wakeup_lat,$pid' in hist_e)

        self.assertTrue('s32 cpu;' in event)
        self.assertTrue('s32 prio;' in event)
        hist_s = synth.repr(event=False, hist_start=True, hist_end=False)
        split_1 = hist_s.split('__arg_')
        arg1 = '__arg_' + split_1[1].split('=')[0]
        arg2 = '__arg_' + split_1[2].split('=')[0]
        self.assertTrue(arg1 + '=target_cpu' in hist_s)
        self.assertTrue(arg2 + '=prio' in hist_s)
        hist_e = synth.repr(event=False, hist_start=False, hist_end=True)
        self.assertTrue('cpu=$' + arg1 in hist_e)
        self.assertTrue('prio=$' + arg2 in hist_e)
        split_2 = hist_e.split('trace(')
        self.assertTrue('$pid' in split_2[1])
        self.assertTrue('$prio' in split_2[1])

        event = synth.repr(event=True, hist_start=False, hist_end=False)
        self.assertTrue('s32 prev_prio;' in event)
        self.assertTrue('s32 nxt_p;' in event)
        hist_e = synth.repr(event=False, hist_start=False, hist_end=True)
        self.assertTrue('nxt_p=next_prio' in hist_e)
        split_3 = hist_e.split('__arg_')
        arg3 = '__arg_' + split_3[3].split('=')[0]
        self.assertTrue(arg3 + '=prev_prio' in hist_e)
        split_4 = hist_e.split('trace(')
        self.assertTrue('$nxt_p' in split_4[1])
        self.assertTrue('$' + arg3 in split_4[1])

        synth.unregister()

    def test_synt_enable(self):
        synth = ft.synth(name='wakeup_lat',
                         start_sys='sched', start_evt='sched_waking',
                         end_sys='sched',   end_evt='sched_switch',
                         start_match='pid', end_match='next_pid',
                         match_name='pid')
        synth.register()
        ret = synth.is_enabled()
        self.assertEqual(ret, '0')
        synth.enable()
        ret = synth.is_enabled()
        self.assertEqual(ret, '1')
        synth.disable()
        ret = synth.is_enabled()
        self.assertEqual(ret, '0')
        synth.unregister()

    def test_synt_enable(self):
        evt_filter = 'prio<100'
        synth = ft.synth(name='wakeup_lat',
                         start_sys='sched', start_evt='sched_waking',
                         end_sys='sched',   end_evt='sched_switch',
                         start_match='pid', end_match='next_pid',
                         match_name='pid')
        synth.add_start_fields(fields=['prio'])
        synth.register()
        self.assertEqual('none', synth.get_filter())
        synth.set_filter(filter=evt_filter)
        self.assertEqual(evt_filter, synth.get_filter())
        synth.clear_filter()
        self.assertEqual('none', synth.get_filter())
        synth.unregister()

swaking = tc.tc_event('sched', 'sched_waking')
sswitch = tc.tc_event('sched', 'sched_switch')

class SynthOopTestCase(unittest.TestCase):
    def test_field_deltaT(self):
        f =  tc.synth_field_deltaT()
        self.assertEqual(f, 'delta_t delta_T')
        f =  tc.synth_field_deltaT(hd=True)
        self.assertEqual(f, 'delta_t delta_T hd')
        f =  tc.synth_field_deltaT(name='dT', hd=True)
        self.assertEqual(f, 'delta_t dT hd')
        f =  tc.synth_field_deltaT(name='dT', hd=False)
        self.assertEqual(f, 'delta_t dT')

    def test_field_delta_start(self):
        f =  tc.synth_field_delta_start('dS', 'foo', 'bar')
        self.assertEqual(f, 'delta_start dS foo bar')

    def test_field_delta_end(self):
        f =  tc.synth_field_delta_end('dE', 'foo', 'bar')
        self.assertEqual(f, 'delta_end dE foo bar')

    def test_field_sum(self):
        f =  tc.synth_field_sum('Sm', 'foo', 'bar')
        self.assertEqual(f, 'sum Sm foo bar')

    def test_synt_create(self):
        start = tc.synth_event_item(event=swaking,
                                    fields=['target_cpu', 'prio'],
                                    match='pid')
        self.assertEqual(start['fields'], ['target_cpu', 'prio'])
        self.assertEqual(start['match'], 'pid')
        self.assertEqual(start['field_names'], [None, None])

        start = tc.synth_field_rename(start,
                                      field='target_cpu', name='cpu')
        self.assertEqual(start['field_names'], ['cpu', None])

        end = tc.synth_event_item(event=sswitch,
                                  fields=['prev_prio'],
                                  match='next_pid')
        self.assertEqual(end['fields'], ['prev_prio'])
        self.assertEqual(end['match'], 'next_pid')

        synth = tc.tc_synth(name='synth_wakeup',
                            start_event=start, end_event=end,
                            synth_fields=[tc.synth_field_deltaT(hd=True)],
                            match_name='pid')

        synth_str = synth.__repr__().split('\n')
        event = synth_str[0]
        hist_s = synth_str[1]
        hist_e = synth_str[2]

        self.assertTrue('keys=pid'in hist_s)
        self.assertTrue('keys=next_pid' in hist_e)
        self.assertTrue('pid=next_pid' in hist_e)
        self.assertTrue('onmatch(sched.sched_waking).trace(synth_wakeup,$pid' in hist_e)
        self.assertTrue('s32 cpu;' in event)
        self.assertTrue('s32 prio;' in event)
        split_1 = hist_s.split('__arg_')
        arg1 = '__arg_' + split_1[1].split('=')[0]
        arg2 = '__arg_' + split_1[2].split('=')[0]
        self.assertTrue(arg1 + '=target_cpu' in hist_s)
        self.assertTrue(arg2 + '=prio' in hist_s)
        self.assertTrue('cpu=$' + arg1 in hist_e)
        self.assertTrue('prio=$' + arg2 in hist_e)
        split_2 = hist_e.split('trace(')
        self.assertTrue('$pid' in split_2[1])
        self.assertTrue('$prio' in split_2[1])
        self.assertTrue('s32 prev_prio;' in event)
        split_3 = hist_e.split('__arg_')
        arg3 = '__arg_' + split_3[3].split('=')[0]
        self.assertTrue(arg3 + '=prev_prio' in hist_e)
        split_4 = hist_e.split('trace(')
        self.assertTrue('$' + arg3 in split_4[1])


if __name__ == '__main__':
    unittest.main()

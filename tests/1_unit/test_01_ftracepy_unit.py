#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import os
import sys
import unittest
import tracecruncher.ftracepy as ft

instance_name = 'test_instance1'
another_instance_name = 'test_instance2'

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

        err='Failed to get local events from \'no_dir\''
        with self.assertRaises(Exception) as context:
            tep.init_local(dir='no_dir', systems=['sched', 'irq']);
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
        systems = ft.available_event_systems()
        self.assertTrue(len(systems) > 1)
        self.assertTrue('sched' in systems)

        inst = ft.create_instance(instance_name)
        systems = ft.available_event_systems(inst)
        self.assertTrue(len(systems) > 1)
        self.assertTrue('sched' in systems)

    def test_available_system_events(self):
        events = ft.available_system_events(system='sched')
        self.assertTrue(len(events) > 1)
        self.assertTrue('sched_switch' in events)

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
                         events='all')

        ret = ft.event_is_enabled(instance=inst,
                                  event='all')
        self.assertEqual(ret, '1')
        ft.disable_events(instance=inst,
                          events='all')

        ret = ft.event_is_enabled(instance=inst,
                                  event='all')
        self.assertEqual(ret, '0')

        ft.enable_events(instance=inst,
                         systems=['sched', 'irq'])

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=inst,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '1')

        ft.disable_events(instance=inst,
                          systems=['sched', 'irq'])

        ret = ft.event_is_enabled(instance=inst,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=inst,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '0')

        ft.enable_events(instance=inst,
                         systems=['sched', 'irq'],
                         events=[['sched_switch', 'sched_waking'],
                                 ['all']])

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
                          systems=['sched', 'irq'],
                          events=[['sched_switch', 'sched_waking'],
                                  ['all']])

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
                             systems=['sched'],
                             events=['all'])
        self.assertTrue(err in str(context.exception))

        err = 'Failed to enable events for unspecified system'
        with self.assertRaises(Exception) as context:
            ft.enable_events(instance=inst,
                             events=['sched_switch', 'sched_wakeup'])
        self.assertTrue(err in str(context.exception))

        err = 'Failed to enable/disable event'
        with self.assertRaises(Exception) as context:
            ft.enable_events(instance=inst,
                             systems=['sched'],
                             events=[['no_event']])
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
    def test_register_kprobe(self):
        evt1 = 'mkdir'
        evt1_func = 'do_mkdirat'
        evt1_prove = 'path=+u0($arg2):ustring'
        evt2 = 'open'
        evt2_func = 'do_sys_openat2'
        evt2_prove = 'file=+u0($arg2):ustring'

        kp1 = ft.register_kprobe(event=evt1, function=evt1_func,
                                 probe=evt1_prove)
        self.assertEqual(evt1, kp1.event())
        self.assertEqual(evt1_func, kp1.function())
        self.assertEqual(evt1_prove, kp1.probe())

        kp2 = ft.register_kprobe(event=evt2, function=evt2_func,
                                 probe=evt2_prove)
        self.assertEqual(evt2, kp2.event())
        self.assertEqual(evt2_func, kp2.function())
        self.assertEqual(evt2_prove, kp2.probe())


    def test_enable_kprobe(self):
        evt1 = 'mkdir'
        evt1_func = 'do_mkdirat'
        evt1_prove = 'path=+u0($arg2):ustring'

        kp1 = ft.register_kprobe(event=evt1, function=evt1_func,
                                 probe=evt1_prove)
        inst = ft.create_instance(instance_name)
        kp1.enable(instance=inst)
        ret = kp1.is_enabled(instance=inst)
        self.assertEqual(ret, '1')

        kp1.disable(instance=inst)
        ret = kp1.is_enabled(instance=inst)
        self.assertEqual(ret, '0')


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


if __name__ == '__main__':
    unittest.main()

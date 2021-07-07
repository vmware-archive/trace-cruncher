#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright (C) 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
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
        ft.create_instance(instance_name)
        self.assertTrue(ft.is_tracing_ON(instance_name))
        instances_dir = ft.dir() + '/instances/'
        self.assertTrue(os.path.isdir(instances_dir + instance_name))

        auto_inst = ft.create_instance(tracing_on=False)
        self.assertFalse(ft.is_tracing_ON(auto_inst))
        ft.destroy_instance(auto_inst)

    def test_destroy_instance(self):
        ft.destroy_instance(instance_name)
        instances_dir = ft.dir() + '/instances/'
        self.assertFalse(os.path.isdir(instances_dir + instance_name))

        err = 'Unable to destroy trace instances'
        with self.assertRaises(Exception) as context:
            ft.destroy_instance(instance_name)
        self.assertTrue(err in str(context.exception))

        ft.create_instance(instance_name)
        ft.create_instance(another_instance_name)
        ft.destroy_all_instances()
        self.assertFalse(os.path.isdir(instances_dir + instance_name))

        ft.create_instance(instance_name)
        ft.create_instance(another_instance_name)
        ft.destroy_instance('all')
        self.assertFalse(os.path.isdir(instances_dir + instance_name))

    def test_get_all(self):
        ft.create_instance(instance_name)
        ft.create_instance(another_instance_name)
        self.assertEqual(ft.get_all_instances(),
                         [instance_name, another_instance_name])
        ft.destroy_all_instances()

    def test_instance_dir(self):
        ft.create_instance(instance_name)
        tracefs_dir = ft.dir();
        instance_dir = tracefs_dir + '/instances/' + instance_name
        self.assertEqual(instance_dir, ft.instance_dir(instance_name))
        ft.destroy_all_instances()

class PyTepTestCase(unittest.TestCase):
    def test_init_local(self):
        tracefs_dir = ft.dir()
        tep = ft.tep_handle();
        tep.init_local(tracefs_dir);

        tep.init_local(dir=tracefs_dir, systems=['sched', 'irq']);

        ft.create_instance(instance_name)
        tracefs_dir = ft.instance_dir(instance_name)
        tep.init_local(dir=tracefs_dir, systems=['sched', 'irq']);

        err='function missing required argument \'dir\''
        with self.assertRaises(Exception) as context:
            tep.init_local(systems=['sched', 'irq']);
        self.assertTrue(err in str(context.exception))

        err='Failed to get local events from \'no_dir\''
        with self.assertRaises(Exception) as context:
            tep.init_local(dir='no_dir', systems=['sched', 'irq']);
        self.assertTrue(err in str(context.exception))
        ft.destroy_all_instances()

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

        ft.create_instance(instance_name)
        systems = ft.available_event_systems(instance_name)
        self.assertTrue(len(systems) > 1)
        self.assertTrue('sched' in systems)

        ft.destroy_all_instances()

    def test_available_system_events(self):
        events = ft.available_system_events(system='sched')
        self.assertTrue(len(events) > 1)
        self.assertTrue('sched_switch' in events)

        ft.create_instance(instance_name)
        events = ft.available_system_events(instance=instance_name,
                                              system='sched')
        self.assertTrue(len(events) > 1)
        self.assertTrue('sched_switch' in events)

        err = 'function missing required argument'
        with self.assertRaises(Exception) as context:
            ft.available_system_events(instance=instance_name)
        self.assertTrue(err in str(context.exception))

        ft.destroy_all_instances()

    def test_enable_event(self):
        ft.create_instance(instance_name)

        ret = ft.event_is_enabled(instance=instance_name, system='all')
        self.assertEqual(ret, '0')
        ft.enable_event(instance=instance_name, system='all')
        ret = ft.event_is_enabled(instance=instance_name, system='all')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=instance_name, system='all')
        ret = ft.event_is_enabled(instance=instance_name, system='all')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=instance_name, event='all')
        self.assertEqual(ret, '0')
        ft.enable_event(instance=instance_name, event='all')
        ret = ft.event_is_enabled(instance=instance_name, event='all')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=instance_name, event='all')
        ret = ft.event_is_enabled(instance=instance_name, event='all')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=instance_name, system='sched')
        self.assertEqual(ret, '0')
        ft.enable_event(instance=instance_name, system='sched')
        ret = ft.event_is_enabled(instance=instance_name, system='sched')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=instance_name, system='sched')
        ret = ft.event_is_enabled(instance=instance_name, system='sched')
        self.assertEqual(ret, '0')

        ft.enable_event(instance=instance_name,
                        system='sched',
                        event='sched_switch')
        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=instance_name,
                         system='sched',
                         event='sched_switch')
        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '0')

        ft.enable_event(instance=instance_name,
                        system='sched',
                        event='all')
        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=instance_name,
                         system='sched',
                         event='all')
        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '0')

        ft.enable_event(instance=instance_name,
                        event='sched_switch')
        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=instance_name,
                         event='sched_switch')
        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '0')

        ft.enable_event(instance=instance_name,
                        system='all',
                        event='sched_switch')
        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '1')
        ft.disable_event(instance=instance_name,
                         system='all',
                         event='sched_switch')
        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '0')

        ft.destroy_all_instances()

    def test_enable_event_err(self):
        ft.create_instance(instance_name)

        err = 'Failed to enable/disable event'
        with self.assertRaises(Exception) as context:
            ft.enable_event(instance=instance_name,
                            system='zero')
        self.assertTrue(err in str(context.exception))

        with self.assertRaises(Exception) as context:
            ft.enable_event(instance=instance_name,
                            system='sched',
                            event='zero')
        self.assertTrue(err in str(context.exception))

        ft.destroy_all_instances()

    def test_enable_events(self):
        ft.create_instance(instance_name)
        ft.enable_events(instance=instance_name,
                         events='all')

        ret = ft.event_is_enabled(instance=instance_name,
                                  event='all')
        self.assertEqual(ret, '1')
        ft.disable_events(instance=instance_name,
                          events='all')

        ret = ft.event_is_enabled(instance=instance_name,
                                  event='all')
        self.assertEqual(ret, '0')

        ft.enable_events(instance=instance_name,
                         systems=['sched', 'irq'])

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '1')

        ft.disable_events(instance=instance_name,
                          systems=['sched', 'irq'])

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='all')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '0')

        ft.enable_events(instance=instance_name,
                         systems=['sched', 'irq'],
                         events=[['sched_switch', 'sched_waking'],
                                 ['all']])

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_waking')
        self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_wakeup')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '1')

        ft.disable_events(instance=instance_name,
                          systems=['sched', 'irq'],
                          events=[['sched_switch', 'sched_waking'],
                                  ['all']])

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_switch')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='sched',
                                  event='sched_waking')
        self.assertEqual(ret, '0')

        ret = ft.event_is_enabled(instance=instance_name,
                                  system='irq',
                                  event='all')
        self.assertEqual(ret, '0')

        ft.destroy_all_instances()

    def test_enable_events_err(self):
        ft.create_instance(instance_name)

        err = 'Inconsistent \"events\" argument'
        with self.assertRaises(Exception) as context:
            ft.enable_events(instance=instance_name,
                             systems=['sched'],
                             events=['all'])
        self.assertTrue(err in str(context.exception))

        err = 'Failed to enable events for unspecified system'
        with self.assertRaises(Exception) as context:
            ft.enable_events(instance=instance_name,
                             events=['sched_switch', 'sched_wakeup'])
        self.assertTrue(err in str(context.exception))

        err = 'Failed to enable/disable event'
        with self.assertRaises(Exception) as context:
            ft.enable_events(instance=instance_name,
                             systems=['sched'],
                             events=[['no_event']])
        self.assertTrue(err in str(context.exception))

        ft.destroy_all_instances()


class OptionsTestCase(unittest.TestCase):
    def test_enable_option(self):
        ft.create_instance(instance_name)
        opt = 'event-fork'
        ret = ft.option_is_set(instance=instance_name,
                                   option=opt)
        self.assertFalse(ret)

        ft.enable_option(instance=instance_name,
                         option=opt)
        ret = ft.option_is_set(instance=instance_name,
                               option=opt)
        self.assertTrue(ret)

        ft.disable_option(instance=instance_name,
                          option=opt)
        ret = ft.option_is_set(instance=instance_name,
                                   option=opt)
        self.assertFalse(ret)

        opt = 'no-opt'
        err = 'Failed to set option \"no-opt\"'
        with self.assertRaises(Exception) as context:
            ft.enable_option(instance=instance_name,
                             option=opt)
        self.assertTrue(err in str(context.exception))

        ft.destroy_all_instances()

    def test_supported_options(self):
        ft.create_instance(instance_name)
        opts = ft.supported_options(instance_name)
        self.assertTrue(len(opts) > 20)
        self.assertTrue('event-fork' in opts)

        ft.destroy_all_instances()

    def test_enabled_options(self):
        ft.create_instance(instance_name)
        opts = ft.enabled_options(instance_name)
        n = len(opts)
        ft.enable_option(instance=instance_name, option='function-fork')
        ft.enable_option(instance=instance_name, option='event-fork')
        opts = ft.enabled_options(instance_name)

        self.assertEqual(len(opts), n + 2)
        self.assertTrue('event-fork' in opts)
        self.assertTrue('function-fork' in opts)

        ft.destroy_all_instances()


class TracingOnTestCase(unittest.TestCase):
    def test_ON_OF(self):
        ft.tracing_ON()
        self.assertTrue(ft.is_tracing_ON())
        ft.tracing_OFF()

        ft.create_instance(instance_name)
        ft.tracing_ON(instance=instance_name)
        self.assertTrue(ft.is_tracing_ON(instance=instance_name))
        self.assertFalse(ft.is_tracing_ON())
        ft.tracing_OFF(instance=instance_name)

        ft.destroy_all_instances()

    def test_err(self):
        err = 'returned a result with an error set'
        with self.assertRaises(Exception) as context:
            ft.tracing_ON('zero')
        self.assertTrue(err in str(context.exception))

        with self.assertRaises(Exception) as context:
            ft.tracing_OFF('zero')
        self.assertTrue(err in str(context.exception))


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright (C) 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import os
import sys
import unittest
import tracecruncher.ftracepy as ft

class InstanceTestCase(unittest.TestCase):
    def test_create_instance(self):
        tracefs_dir = ft.dir()
        instances_dir = tracefs_dir + '/instances/'
        self.assertEqual(len(os.listdir(instances_dir)), 0)

        for i in range(25) :
            instance_name = 'test_instance_%s' % i
            ft.create_instance(instance_name)
            self.assertTrue(os.path.isdir(instances_dir + instance_name))

        for i in range(15) :
            instance_name = 'test_instance_%s' % i
            ft.destroy_instance(instance_name)
            self.assertFalse(os.path.isdir(instances_dir + instance_name))

        self.assertEqual(len(os.listdir(instances_dir)), 10)
        ft.destroy_instance('all')
        self.assertEqual(len(os.listdir(instances_dir)), 0)

    def test_current_tracer(self):
        current = ft.get_current_tracer()
        self.assertEqual(current, 'nop')
        ft.tracing_OFF()
        name = 'function'
        ft.set_current_tracer(tracer=name)
        current = ft.get_current_tracer()
        self.assertEqual(current, name)
        ft.set_current_tracer()

        instance_name = 'test_instance'
        ft.create_instance(instance_name)
        current = ft.get_current_tracer(instance=instance_name)
        self.assertEqual(current, 'nop')
        ft.tracing_OFF(instance=instance_name)
        ft.set_current_tracer(instance=instance_name, tracer=name)
        current = ft.get_current_tracer(instance=instance_name)
        self.assertEqual(current, name)
        ft.destroy_instance('all')

    def test_enable_events(self):
        instance_name = 'test_instance'
        ft.create_instance(instance_name)
        systems = ft.available_event_systems(instance=instance_name)
        systems.remove('ftrace')
        for s in systems:
            ret = ft.event_is_enabled(instance=instance_name,
                                       system=s)
            self.assertEqual(ret, '0')
            ft.enable_event(instance=instance_name,
                             system=s)
            ret = ft.event_is_enabled(instance=instance_name,
                                       system=s)
            self.assertEqual(ret, '1')

            ft.disable_event(instance=instance_name,
                             system=s)
            events = ft.available_system_events(instance=instance_name,
                                                 system=s)
            for e in events:
                ret = ft.event_is_enabled(instance=instance_name,
                                           system=s,
                                           event=e)
                self.assertEqual(ret, '0')
                ft.enable_event(instance=instance_name,
                                 system=s,
                                 event=e)
                ret = ft.event_is_enabled(instance=instance_name,
                                           system=s,
                                           event=e)
                self.assertEqual(ret, '1')
                ret = ft.event_is_enabled(instance=instance_name,
                                           system=s)
                if e != events[-1]:
                    self.assertEqual(ret, 'X')

            ret = ft.event_is_enabled(instance=instance_name,
                                       system=s)
            self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=instance_name,
                                   system=s)
        self.assertEqual(ret, '1')

        ft.disable_event(instance=instance_name, event='all')
        for s in systems:
            ret = ft.event_is_enabled(instance=instance_name,
                                       system=s)
            self.assertEqual(ret, '0')
            events = ft.available_system_events(instance=instance_name,
                                                 system=s)
            for e in events:
                ret = ft.event_is_enabled(instance=instance_name,
                                           system=s,
                                           event=e)
                self.assertEqual(ret, '0')


if __name__ == '__main__':
    unittest.main()

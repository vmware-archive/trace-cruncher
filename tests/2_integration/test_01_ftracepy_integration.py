#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
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
            inst = ft.create_instance(instance_name)
            self.assertTrue(os.path.isdir(instances_dir + instance_name))

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
        inst = ft.create_instance(instance_name)
        current = ft.get_current_tracer(instance=inst)
        self.assertEqual(current, 'nop')
        ft.tracing_OFF(instance=inst)
        ft.set_current_tracer(instance=inst, tracer=name)
        current = ft.get_current_tracer(instance=inst)
        self.assertEqual(current, name)

    def test_enable_events(self):
        instance_name = 'test_instance'
        inst = ft.create_instance(instance_name)
        systems = ft.available_event_systems(instance=inst)
        if 'ftrace' in systems:
            systems.remove('ftrace')
        for s in systems:
            ret = ft.event_is_enabled(instance=inst,
                                       system=s)
            self.assertEqual(ret, '0')
            ft.enable_event(instance=inst,
                             system=s)
            ret = ft.event_is_enabled(instance=inst,
                                       system=s)
            self.assertEqual(ret, '1')

            ft.disable_event(instance=inst,
                             system=s)
            events = ft.available_system_events(instance=inst,
                                                 system=s)
            for e in events:
                ret = ft.event_is_enabled(instance=inst,
                                           system=s,
                                           event=e)
                self.assertEqual(ret, '0')
                ft.enable_event(instance=inst,
                                 system=s,
                                 event=e)
                ret = ft.event_is_enabled(instance=inst,
                                           system=s,
                                           event=e)
                self.assertEqual(ret, '1')
                ret = ft.event_is_enabled(instance=inst,
                                           system=s)
                if e != events[-1]:
                    self.assertEqual(ret, 'X')

            ret = ft.event_is_enabled(instance=inst,
                                       system=s)
            self.assertEqual(ret, '1')

        ret = ft.event_is_enabled(instance=inst,
                                   system=s)
        self.assertEqual(ret, '1')

        ft.disable_event(instance=inst, event='all')
        for s in systems:
            ret = ft.event_is_enabled(instance=inst,
                                       system=s)
            self.assertEqual(ret, '0')
            events = ft.available_system_events(instance=inst,
                                                 system=s)
            for e in events:
                ret = ft.event_is_enabled(instance=inst,
                                           system=s,
                                           event=e)
                self.assertEqual(ret, '0')


if __name__ == '__main__':
    unittest.main()

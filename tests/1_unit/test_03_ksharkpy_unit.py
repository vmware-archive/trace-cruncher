#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import os
import sys
import unittest
import tracecruncher.ksharkpy as ks
import tracecruncher.npdatawrapper as dw

file_1 = 'testdata/trace_test1.dat'
file_2 = 'testdata/trace_test2.dat'

ss_id = 323

class KsPyTestCase(unittest.TestCase):
    def test_open_close(self):
        sd = ks.open(file_1)
        self.assertEqual(sd, 0)
        sd = ks.open(file_2)
        self.assertEqual(sd, 1)
        ks.close()

        sd = ks.open(file_1)
        self.assertEqual(sd, 0)
        ks.close()

        err = 'Failed to open file'
        with self.assertRaises(Exception) as context:
            sd = ks.open('no_file')
        self.assertTrue(err in str(context.exception))

    def test_event_id(self):
        sd = ks.open(file_1)
        eid = ks.event_id(stream_id=sd, name='sched/sched_switch')
        self.assertEqual(eid, ss_id)

        err = 'Failed to retrieve the Id of event'
        with self.assertRaises(Exception) as context:
            eid = ks.event_id(stream_id=sd, name='sched/no_such_event')
        self.assertTrue(err in str(context.exception))

        ks.close()

    def test_event_name(self):
        sd = ks.open(file_1)
        name = ks.event_name(stream_id=sd, event_id=ss_id)
        self.assertEqual(name, 'sched/sched_switch')

        err = 'Failed to retrieve the name of event'
        with self.assertRaises(Exception) as context:
            name = ks.event_name(stream_id=sd, event_id=2**30)
        self.assertTrue(err in str(context.exception))

        ks.close()

    def read_field(self):
        sd = ks.open(file_1)
        data = dw.load(sd)
        next_pid = read_event_field(stream_id=sd,
                                    offset=data['offset'],
                                    event_id=ss_id,
                                    field='next_pid')
        self.assertEqual(next_pid, 4182)


if __name__ == '__main__':
    unittest.main()

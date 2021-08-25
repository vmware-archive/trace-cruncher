#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright (C) 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import os
import sys
import unittest
import tracecruncher.ksharkpy as ks
import tracecruncher.npdatawrapper as dw

file_1 = 'testdata/trace_test1.dat'

class DwPyTestCase(unittest.TestCase):
    def test_columns(self):
        self.assertEqual(dw.columns(), ['event', 'cpu', 'pid', 'offset', 'time'])

    def test_load(self):
        sd = ks.open(file_1)
        data = dw.load(sd)
        self.assertEqual(len(dw.columns()), len(data))
        self.assertEqual(data['pid'].size, 1530)

        data_no_ts = dw.load(sd, ts_data=False)
        self.assertEqual(data_no_ts['pid'].size, 1530)
        self.assertEqual(len(dw.columns()) - 1, len(data_no_ts))

        data_pid_cpu = dw.load(sd, evt_data=False,
                                   ofst_data=False,
                                   ts_data=False)
        self.assertEqual(data_pid_cpu['cpu'].size, 1530)
        self.assertEqual(2, len(data_pid_cpu))

        ks.close()


if __name__ == '__main__':
    unittest.main()

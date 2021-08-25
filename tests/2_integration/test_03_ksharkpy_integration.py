#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import os
import sys
import shutil
import unittest
import tracecruncher.ks_utils as tc

class GetTestData(unittest.TestCase):
    def test_open_and_read(self):
        f = tc.open_file(file_name='testdata/trace_test1.dat')
        data = f.load(pid_data=False)
        tasks = f.get_tasks()
        self.assertEqual(len(tasks), 29)
        self.assertEqual(tasks['zoom'], [28121, 28137, 28141, 28199, 28201, 205666])


if __name__ == '__main__':
    unittest.main()

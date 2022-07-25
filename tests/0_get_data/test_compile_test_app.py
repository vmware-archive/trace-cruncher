#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2022 VMware Inc, Tzvetomir Stoyanov (VMware) <tz.stoyanov@gmail.com>
"""

import os
import sys
import unittest
import subprocess

class CompileTestApp(unittest.TestCase):
    def test_compile_app(self):
        test_app = 'testapp/tc-test-app'
        self.assertTrue(os.path.exists(test_app + ".c"))
        self.assertTrue(os.path.isfile(test_app + ".c"))
        if os.path.exists(test_app):
            self.assertTrue(os.path.isfile(test_app))
            os.remove(test_app)
        cmd = ["gcc", test_app + ".c", "-lrt", "-o", test_app];
        p = subprocess.Popen(cmd);
        p.wait();
        self.assertTrue(p.returncode == 0)

if __name__ == '__main__':
    unittest.main()

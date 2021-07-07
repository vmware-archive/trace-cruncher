#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright (C) 2021 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

import os
import sys
import shutil
import unittest
import git

class GetTestData(unittest.TestCase):
    def test_get_data(self):
        data_dir = 'testdata'
        if os.path.exists(data_dir) and os.path.isdir(data_dir):
            shutil.rmtree(data_dir)

        github_repo = 'https://github.com/yordan-karadzhov/kernel-shark_testdata.git'
        repo = git.Repo.clone_from(github_repo, data_dir)


if __name__ == '__main__':
    unittest.main()

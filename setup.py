#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""
import sys

from setuptools import setup, find_packages
from distutils.core import Extension
from Cython.Build import cythonize

import pkgconfig as pkg
import numpy as np

def version_check(lib, lib_version, min_version):
    min_v = min_version.split('.')
    lib_v = pkg.modversion(lib).split('.')
    if lib_v[2] == 'dev':
        lib_v[2] = '0'

    if tuple(map(int, lib_v)) < tuple(map(int, min_v)):
        print('{0} v{1} is required'.format(lib, min_version))
        return False
    return True

def add_library(lib, min_version,
                libs_found,
                library_dirs,
                include_dirs):
    pkg_info = pkg.parse(lib)
    lib_version = pkg.modversion(lib)
    if version_check(lib, lib_version, min_version):
        include_dirs.extend(pkg_info['include_dirs'])
        library_dirs.extend(pkg_info['library_dirs'])
        libs_found.extend([(lib, lib_version)])

def third_party_paths():
    library_dirs = ['$ORIGIN','tracecruncher']
    include_dirs = [np.get_include()]
    libs_required = [('libtraceevent', '1.5.0'),
                     ('libtracefs',    '1.3.0'),
                     ('libkshark',     '2.0.1')]
    libs_found = []

    for lib in libs_required:
        add_library(lib[0], lib[1],
                    libs_found,
                    library_dirs,
                    include_dirs)

    if len(libs_found) < len(libs_required):
        sys.exit(1)

    library_dirs = list(set(library_dirs))

    return include_dirs, library_dirs

include_dirs, library_dirs = third_party_paths()

def extension(name, sources, libraries):
    runtime_library_dirs = library_dirs
    return Extension(name, sources=sources,
                           include_dirs=include_dirs,
                           library_dirs=library_dirs,
                           runtime_library_dirs=runtime_library_dirs,
                           libraries=libraries,
                           )

def main():
    module_ft = extension(name='tracecruncher.ftracepy',
                          sources=['src/ftracepy.c', 'src/ftracepy-utils.c', 'src/trace-obj-debug.c'],
                          libraries=['traceevent', 'tracefs', 'bfd'])

    cythonize('src/npdatawrapper.pyx', language_level = 3)
    module_data = extension(name='tracecruncher.npdatawrapper',
                            sources=['src/npdatawrapper.c'],
                            libraries=['kshark'])

    module_ks = extension(name='tracecruncher.ksharkpy',
                          sources=['src/ksharkpy.c', 'src/ksharkpy-utils.c'],
                          libraries=['kshark'])

    setup(name='tracecruncher',
          version='0.2.0',
          description='Interface for accessing Linux tracing data in Python.',
          author='Yordan Karadzhov (VMware)',
          author_email='y.karadz@gmail.com',
          url='https://github.com/vmware/trace-cruncher',
          license='LGPL-2.1',
          packages=find_packages(),
          package_data={'tracecruncher': ['*.so']},
          ext_modules=[module_ft, module_data, module_ks],
          classifiers=[
              'Development Status :: 4 - Beta',
              'Programming Language :: Python :: 3',
              ]
          )


if __name__ == '__main__':
    main()

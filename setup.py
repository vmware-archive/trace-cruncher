#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
"""

from setuptools import setup, find_packages
from distutils.core import Extension
from Cython.Build import cythonize

import pkgconfig as pkg


def third_party_paths():
    pkg_traceevent = pkg.parse('libtraceevent')
    pkg_ftracepy = pkg.parse('libtracefs')
    pkg_tracecmd = pkg.parse('libtracecmd')

    include_dirs = []
    include_dirs.extend(pkg_traceevent['include_dirs'])
    include_dirs.extend(pkg_ftracepy['include_dirs'])
    include_dirs.extend(pkg_tracecmd['include_dirs'])

    library_dirs = []
    library_dirs.extend(pkg_traceevent['library_dirs'])
    library_dirs.extend(pkg_ftracepy['library_dirs'])
    library_dirs.extend(pkg_tracecmd['library_dirs'])
    library_dirs = list(set(library_dirs))

    return include_dirs, library_dirs

include_dirs, library_dirs = third_party_paths()

def extension(name, sources, libraries):
    runtime_library_dirs = library_dirs
    runtime_library_dirs.extend('$ORIGIN')
    return Extension(name, sources=sources,
                           include_dirs=include_dirs,
                           library_dirs=library_dirs,
                           runtime_library_dirs=runtime_library_dirs,
                           libraries=libraries,
                           )

def main():
    module_ft = extension(name='tracecruncher.ftracepy',
                          sources=['src/ftracepy.c', 'src/ftracepy-utils.c'],
                          libraries=['traceevent', 'tracefs'])

    setup(name='tracecruncher',
          version='0.1.0',
          description='NumPy based interface for accessing tracing data in Python.',
          author='Yordan Karadzhov (VMware)',
          author_email='y.karadz@gmail.com',
          url='https://github.com/vmware/trace-cruncher',
          license='LGPL-2.1',
          packages=find_packages(),
          ext_modules=[module_ft],
          classifiers=[
              'Development Status :: 3 - Alpha',
              'Programming Language :: Python :: 3',
              ]
          )


if __name__ == '__main__':
    main()

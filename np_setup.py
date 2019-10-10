#!/usr/bin/env python3

"""
SPDX-License-Identifier: LGPL-2.1

Copyright 2019 VMware Inc, Yordan Karadzhov <ykaradzhov@vmware.com>
"""

import sys
import getopt

from Cython.Distutils import build_ext
from numpy.distutils.misc_util import Configuration
from numpy.distutils.core import setup

def lib_dirs(argv):
    """ Function used to retrieve the library paths.
    """
    kslibdir = ''
    evlibdir = ''
    trlibdir = ''

    try:
        opts, args = getopt.getopt(
            argv, 'k:t:e:', ['kslibdir=',
                             'trlibdir=',
                             'evlibdir='])

    except getopt.GetoptError:
        sys.exit(2)

    for opt, arg in opts:
        if opt in ('-k', '--kslibdir'):
            kslibdir = arg
        elif opt in ('-t', '--trlibdir'):
            trlibdir = arg
        elif opt in ('-e', '--evlibdir'):
            evlibdir = arg

    cmd1 = 1
    for i in range(len(sys.argv)):
        if sys.argv[i] == 'build_ext':
            cmd1 = i

    sys.argv = sys.argv[:1] + sys.argv[cmd1:]

    if kslibdir == '':
        kslibdir = '/usr/local/lib/kernelshark'

    if evlibdir == '':
        evlibdir = '/usr/local/lib/traceevent'

    if trlibdir == '':
        trlibdir = '/usr/local/lib/trace-cmd/'

    return [kslibdir, evlibdir, trlibdir]


def configuration(parent_package='',
                  top_path=None,
                  libs=['kshark', 'tracecmd', 'traceevent', 'json-c'],
                  libdirs=['.']):
    """ Function used to build configuration.
    """
    config = Configuration('', parent_package, top_path)
    config.add_extension('ksharkpy',
                         sources=['libkshark_wrapper.pyx'],
                         libraries=libs,
                         define_macros=[('KS_PLUGIN_DIR','\"' + libdirs[0] + '/plugins\"')],
                         library_dirs=libdirs,
                         depends=['libkshark-py.c'],
                         include_dirs=libdirs)

    return config


def main(argv):
    # Retrieve third-party libraries.
    libdirs = lib_dirs(sys.argv[1:])

    # Retrieve the parameters of the configuration.
    params = configuration(libdirs=libdirs).todict()
    params['cmdclass'] = dict(build_ext=build_ext)

    ## Building the extension.
    setup(**params)


if __name__ == '__main__':
    main(sys.argv[1:])

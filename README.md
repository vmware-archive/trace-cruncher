

# trace-cruncher

## Overview

The Trace-Cruncher project aims to provide an interface between the existing instrumentation for collection and visualization of tracing data from the Linux kernel and the broad and very well developed ecosystem of instruments for data analysis available in Python. The interface will be based on NumPy.

NumPy implements an efficient multi-dimensional container of generic data and uses strong typing in order to provide fast data processing in Python. The  Trace-Cruncher will allow for sophisticated analysis of kernel tracing data via scripts, but it will also opens the door for exposing the kernel tracing data to the instruments provided by the scientific toolkit of Python like MatPlotLib, Stats, Scikit-Learn and even to the nowadays most popular frameworks for Machine Learning like TensorFlow and PyTorch. The Trace-Cruncher is strongly coupled to the KernelShark project and is build on top of the C API of libkshark.

## Try it out

### Prerequisites

Trace-Cruncher has the following external dependencies:
  trace-cmd / KernelShark, Json-C, Cython, NumPy, MatPlotLib.

1.1 In order to install the packages on Ubuntu do the following:
    sudo apt-get install libjson-c-dev libpython3-dev cython3 -y
    sudo apt-get install python3-numpy python3-matplotlib -y

1.2 In order to install the packages on Fedora, as root do the following:
    dnf install json-c-devel python3-devel python3-Cython -y
    dnf install python3-numpy python3-matplotlib -y

2. In order to get the proper version of KernelShark / trace-cmd do the
following:
    git clone git://git.kernel.org/pub/scm/utils/trace-cmd/trace-cmd.git --branch=kernelshark-v1.1

or download a tarball from here:
https://git.kernel.org/pub/scm/utils/trace-cmd/trace-cmd.git/snapshot/trace-cmd-kernelshark-v1.1.tar.gz

### Build & Run

1. Patch trace-cmd / KernelShark:
    cd path/to/trace-cmd/
    git am ../path/to/kshark-py/0001-kernel-shark-Add-_DEVEL-build-flag.patch

2. Install trace-cmd:
    make
    sudo make install_libs

3. Install KernelShark:
    cd kernel-shark/build
    cmake -D_DEVEL=1 ../
    make
    sudo make install

4. Build the NumPy API itself:
    cd path/to/trace-cruncher
    ./np_setup.py build_ext -i

## Documentation

For bug reports and issues, please file it here:

https://bugzilla.kernel.org/buglist.cgi?component=Trace-cmd%2FKernelshark&product=Tools&resolution=---

## Contributing

The trace-cruncher project team welcomes contributions from the community. If you wish to contribute code and you have not signed our contributor license agreement (CLA), our bot will update the issue when you open a Pull Request. For any questions about the CLA process, please refer to our [FAQ](https://cla.vmware.com/faq).

## License

This is available under the [LGPLv2.1 license](COPYING-LGPLv2.1.txt).

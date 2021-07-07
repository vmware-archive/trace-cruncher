

# trace-cruncher

## Overview

The Trace-Cruncher project aims to provide an interface between the existing instrumentation for collection and visualization of tracing data from the Linux kernel and the broad and very well developed ecosystem of instruments for data analysis available in Python. The interface is based on NumPy.

NumPy implements an efficient multi-dimensional container of generic data and uses strong typing in order to provide fast data processing in Python. The  Trace-Cruncher allows for sophisticated analysis of kernel tracing data via scripts, but it also opens the door for exposing the kernel tracing data to the instruments provided by the scientific toolkit of Python like MatPlotLib, Stats, Scikit-Learn and even to the nowadays most popular frameworks for Machine Learning like TensorFlow and PyTorch. The Trace-Cruncher is strongly coupled to the KernelShark project and is build on top of the C API of libkshark.

## Try it out

### Prerequisites

Trace-Cruncher has the following external dependencies:
  libtraceevent, libtracefs, KernelShark, Json-C, Cython, NumPy.

1.1 In order to install all packages on Ubuntu do the following:

    > sudo apt-get update

    > sudo apt-get install build-essential git cmake libjson-c-dev -y

    > sudo apt-get install libpython3-dev cython3 python3-numpy python3-pip -y

    > sudo pip3 install --system pkgconfig GitPython

1.2 In order to install all packages on Fedora, as root do the following:

    > dnf install gcc gcc-c++ git cmake json-c-devel -y

    > dnf install python3-devel python3-Cython python3-numpy python3-pip -y

    > sudo pip3 install --system pkgconfig GitPython


2 In order to install all third party libraries do the following:

    > git clone https://git.kernel.org/pub/scm/libs/libtrace/libtraceevent.git/

    > cd libtraceevent

    > make

    > sudo make install

    > cd ..


    > git clone https://git.kernel.org/pub/scm/libs/libtrace/libtracefs.git/

    > cd libtracefs

    > make

    > sudo make install

    > cd ..


    > git clone https://git.kernel.org/pub/scm/utils/trace-cmd/trace-cmd.git

    > cd trace-cmd

    > make

    > sudo make install_libs

    > cd ..


    > git clone https://github.com/yordan-karadzhov/kernel-shark-v2.beta.git kernel-shark

    > cd kernel-shark/build

    > cmake ..

    > make

    > sudo make install

    > cd ../..

### Build & Run

Installing trace-cruncher is very simple. After downloading the source code, you just have to run:

     > cd trace-cruncher

     > make

     > sudo make install

## Documentation

For bug reports and issues, please file it here:

https://bugzilla.kernel.org/buglist.cgi?component=Trace-cmd%2FKernelshark&product=Tools&resolution=---

## Contributing

The trace-cruncher project team welcomes contributions from the community. If you wish to contribute code and you have not signed our contributor license agreement (CLA), our bot will update the issue when you open a Pull Request. For any questions about the CLA process, please refer to our [FAQ](https://cla.vmware.com/faq).

## License

This is available under the [LGPLv2.1 license](COPYING-LGPLv2.1.txt).

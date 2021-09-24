

# trace-cruncher

## Overview

The Trace-Cruncher project aims to provide an interface between the existing instrumentation for collection and visualization of tracing data from the Linux kernel and the broad and very well developed ecosystem of instruments for data analysis available in Python.

The Trace-Cruncher allows for sophisticated analysis of kernel tracing data via scripts, but it also opens the door for exposing the kernel tracing data to the instruments provided by the scientific toolkit of Python like MatPlotLib, Stats, Scikit-Learn and even to the nowadays most popular frameworks for Machine Learning like TensorFlow and PyTorch. The Trace-Cruncher is strongly coupled to the [libtraceevent](https://git.kernel.org/pub/scm/libs/libtrace/libtraceevent.git), [libtracefs](https://git.kernel.org/pub/scm/libs/libtrace/libtracefs.git/) and [KernelShark](https://git.kernel.org/pub/scm/utils/trace-cmd/kernel-shark.git/) projects and is build on top of the C API of this librearies.

## Try it out

### Prerequisites

Trace-Cruncher has the following external dependencies:
  libtraceevent, libtracefs, trace-cmd, KernelShark, Json-C, Cython, NumPy.

1.1 In order to install all packages on Ubuntu do the following:

    > sudo apt-get update

    > sudo apt-get install build-essential git cmake libjson-c-dev -y

    > sudo apt-get install libpython3-dev cython3 python3-numpy python3-pip -y

    > sudo apt-get install flex valgrind -y

    > sudo pip3 install --system pkgconfig GitPython

1.2 In order to install all packages on Fedora, as root do the following:

    > sudo dnf install gcc gcc-c++ git cmake json-c-devel -y

    > sudo dnf install python3-devel python3-Cython python3-numpy python3-pip -y

    > sudo dnf install flex valgrind -y

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


    > git clone https://git.kernel.org/pub/scm/utils/trace-cmd/trace-cmd.git/

    > cd trace-cmd

    > make

    > sudo make install_libs

    > cd ..


    > git clone https://git.kernel.org/pub/scm/utils/trace-cmd/kernel-shark.git/

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
For questions about the use of Trace-Cruncher, please send email to: linux-trace-users@vger.kernel.org

[Subscribe](http://vger.kernel.org/vger-lists.html#linux-trace-users) / [Archives](https://lore.kernel.org/linux-trace-users/)

For bug reports and issues, please file it
[bugzilla](https://bugzilla.kernel.org/buglist.cgi?component=Trace-cmd%2FKernelshark&product=Tools&resolution=---)

## Contributing

The trace-cruncher project team welcomes contributions from the community. For more detailed information, refer to [CONTRIBUTING.md](CONTRIBUTING.md).

## License

This is available under the [LGPLv2.1 license](COPYING-LGPLv2.1.txt).

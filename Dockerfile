# syntax=docker/dockerfile:1
# SPDX-License-Identifier: LGPL-2.1
# Copyright (C) 2022, VMware Inc, June Knauth <june.knauth@gmail.com>

FROM debian
# Install APT and pip dependencies
RUN apt update && apt install build-essential git cmake libjson-c-dev libpython3-dev cython3 python3-numpy python3-pip flex valgrind binutils-dev pkg-config swig curl -y && pip3 install pkgconfig GitPython
# Download the latest date-snapshot tool from the trace-cruncher GitHub
# Then use it to download a snapshot of trace-cruncher and its dependencies (defined in repos)
RUN mkdir build
WORKDIR build
RUN curl -o date-snapshot.sh https://raw.githubusercontent.com/vmware/trace-cruncher/tracecruncher/scripts/date-snapshot/date-snapshot.sh &&\
curl -o repos https://raw.githubusercontent.com/vmware/trace-cruncher/tracecruncher/scripts/date-snapshot/repos &&\
bash ./date-snapshot.sh -i "trace-cruncher;https://github.com/vmware/trace-cruncher.git;tracecruncher;20220628" &&\
bash ./date-snapshot.sh -f repos
# Build kernel tracing libs
RUN cd libtraceevent && make && make install
RUN cd libtracefs && make && make install
RUN cd trace-cmd && make && make install_libs
RUN cd kernel-shark/build && cmake .. && make && make install
# Install trace-cruncher
RUN cd trace-cruncher && make && make install

#!/bin/bash
# trace-cruncher install script
# created by June Knauth (VMware) <june.knauth@gmail.com>, 2022-06-14

# Install Dependencies
echo "---Installing APT Deps---"
sudo apt-get update
sudo apt-get install build-essential git cmake libjson-c-dev -y
sudo apt-get install libpython3-dev cython3 python3-numpy python3-pip -y
sudo apt-get install flex valgrind binutils-dev pkg-config -y
sudo pip3 install --system pkgconfig GitPython

echo "---Building Other Deps---"
git clone https://git.kernel.org/pub/scm/libs/libtrace/libtraceevent.git/
cd libtraceevent
make
sudo make install
cd ..
git clone https://git.kernel.org/pub/scm/libs/libtrace/libtracefs.git/
cd libtracefs
make
sudo make install
cd ..
git clone https://git.kernel.org/pub/scm/utils/trace-cmd/trace-cmd.git
cd trace-cmd
make
sudo make install_libs
cd ..
git clone https://github.com/yordan-karadzhov/kernel-shark-v2.beta.git kernel-shark
cd kernel-shark/build
cmake ..
make
sudo make install
cd ../..

echo "---Installing trace-cruncher---"
make
sudo make install

echo "---Running Unit Tests---"

#!/bin/bash
# Build and run tests with Travis CI.

set -e -x

root=$PWD
mkdir build
cd build

# Configure.
if [ "$CC" == "gcc" ]; then
    # GCC build generates coverage.
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_COMPILER=$COMPILER \
        -DCMAKE_CXX_FLAGS=--coverage \
        -DCMAKE_C_FLAGS=--coverage \
        -DWITH_QT5=OFF \
        ..
else
    qmake CONFIG+=debug QMAKE_CXX=$COMPILER QMAKE_CXXFLAGS="-std=c++11" ..
fi

# Build.
make

# Test command line arguments that don't need GUI.
DISPLAY="" ./copyq --help
DISPLAY="" ./copyq --version
DISPLAY="" ./copyq --info

# Start X11 and window manager.
export DISPLAY=:99.0
sh -e /etc/init.d/xvfb start
sleep 4
openbox &
sleep 8

# Clean up old configuration.
rm -rf ~/.config/copyq.test

# Run tests.
./copyq tests

cd "$root"

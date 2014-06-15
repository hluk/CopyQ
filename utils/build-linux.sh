#!/bin/bash
# Build and run tests with Travis CI.

set -e -x

root=$PWD
mkdir build
cd build

# Configure.
if [ "$CC" == "gcc" ]; then
    # GCC build generates coverage.
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=--coverage -DCMAKE_C_FLAGS=--coverage ..
else
    qmake QMAKE_CXX=clang++ CONFIG+=debug ..
fi

# Build.
make

# Start X11 and window manager.
export DISPLAY=:99.0
sh -e /etc/init.d/xvfb start
sleep 3
openbox &
sleep 3

# Clean up old configuration.
rm -rf ~/.config/copyq.test

# Run tests.
./copyq tests

cd "$root"

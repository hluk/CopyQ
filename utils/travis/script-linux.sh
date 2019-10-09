#!/bin/bash
# Build and run tests with Travis CI.

set -xeuo pipefail

root=$PWD
mkdir build
cd build

if [ "$CC" == "gcc" ]; then
    # GCC build generates coverage.
    cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_COMPILER=$COMPILER \
        -DCMAKE_CXX_FLAGS=--coverage \
        -DCMAKE_C_FLAGS=--coverage \
        ..
else
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=$COMPILER \
        -DWITH_TESTS=ON \
        ..
fi

# Build.
make

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG

# Test command line arguments that don't need GUI.
DISPLAY="" ./copyq --help
DISPLAY="" ./copyq --version
DISPLAY="" ./copyq --info

# Start X11 and window manager.
export DISPLAY=':99.0'
Xvfb :99 -screen 0 1280x960x24 &
sleep 5
openbox &
sleep 8

# Clean up old configuration.
rm -rf ~/.config/copyq.test

# Run tests.
export COPYQ_TESTS_RERUN_FAILED=0
./copyq tests

cd "$root"

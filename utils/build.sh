#!/bin/bash
# Build and run tests with Travis CI.

set -e

root=$PWD
mkdir build
cd build

# Configure.
if [ "$CC" == "gcc" ]; then
    # GCC build generates coverage.
    qmake CONFIG+=debug QMAKE_CXXFLAGS+=--coverage QMAKE_LFLAGS+=--coverage ..
else
    qmake CONFIG+=debug ..
fi

# Build.
make

# Start X11 and window manager.
export DISPLAY=:99.0
sh -e /etc/init.d/xvfb start
sleep 3
openbox &

# Clean up old configuration.
rm -rf ~/.config/copyq.test

# Run tests.
./copyq tests

cd "$root"

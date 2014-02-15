#!/bin/bash
# Build and run tests with Travis CI.

# Configure.
if [ "$CC" == "gcc" ]; then
    # GCC build generates coverage.
    qmake CONFIG+=debug QMAKE_CXXFLAGS+=--coverage QMAKE_LFLAGS+=--coverage
else
    qmake CONFIG+=debug
fi

# Build.
make

# Run tests.
export DISPLAY=:99.0
sh -e /etc/init.d/xvfb start
sleep 3
openbox &
./copyq tests

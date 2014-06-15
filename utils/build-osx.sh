#!/bin/bash
# Build with Travis CI.

set -e -x

root=$PWD
mkdir -p build
cd build

/usr/local/opt/qt5/bin/qmake CONFIG+=release ..
make dmg

cd "$root"

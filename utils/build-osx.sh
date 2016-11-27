#!/bin/bash
# Build with Travis CI.

set -e -x

root=$PWD
mkdir -p build
cd build

/usr/local/opt/qt5/bin/qmake CONFIG+=release ..
make
make package_plugins
make package_translations
/usr/local/opt/qt5/bin/macdeployqt copyq.app -dmg

cd "$root"

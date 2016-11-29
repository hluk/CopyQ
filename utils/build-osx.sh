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

# Create CopyQ.dmg
/usr/local/opt/qt5/bin/macdeployqt CopyQ.app -verbose=2 -dmg

executable="CopyQ.app/Contents/MacOS/copyq"

# Fix dependencies
git clone https://github.com/iltommi/macdeployqtfix.git
python macdeployqtfix/macdeployqtfix.py "$executable" /usr/local/Cellar/qt5

# Test the deployed app
"$executable" --help
"$executable" --version

cd "$root"

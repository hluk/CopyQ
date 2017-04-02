#!/bin/bash
# Build with Travis CI.

set -e -x

root=$PWD
mkdir -p build
cd build

qt_bin="/usr/local/opt/qt5/bin"

"$qt_bin/qmake" CONFIG+=release CONFIG+=tests ..
make
make package_plugins
make package_translations

# Create "CopyQ.app/"
"$qt_bin/macdeployqt" CopyQ.app -verbose=2 -always-overwrite

executable="CopyQ.app/Contents/MacOS/copyq"

# Fix paths for included Qt libraries.
git clone https://github.com/iltommi/macdeployqtfix.git
python macdeployqtfix/macdeployqtfix.py "$executable" /usr/local/Cellar/qt5

# Test the app before deployment.
"$executable" --help
"$executable" --version
"$executable" --info

# Run tests.
"$executable" tests || "$executable" tests

# Create "CopyQ.dmg".
"$qt_bin/macdeployqt" CopyQ.app -verbose=2 -dmg -no-plugins

cd "$root"

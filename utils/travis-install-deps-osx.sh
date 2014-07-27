#!/usr/bin/env bash

set -e -x

brew install qt5 python
pip install cpp-coveralls
pip install dmgbuild 'pyobjc-framework-Quartz==3.0.1' 'pyobjc-framework-Cocoa==3.0.1'

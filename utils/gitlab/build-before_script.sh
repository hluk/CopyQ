#!/bin/bash
set -ex

apt update

# Build dependencies
apt -y install g++ cmake make git
apt -y install qtbase5-private-dev qtscript5-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev libqt5x11extras5-dev

# Optional: Better support for X11
apt -y install libxfixes-dev libxtst-dev

# Optional: CMake can get version from git
apt -y install git

#!/bin/bash
set -ex

packages=(
    g++
    cmake
    make
    git

    qt5-default

    qtscript5-dev
    qttools5-dev
    qttools5-dev-tools

    libqt5x11extras5-dev

    libqt5svg5-dev
    libqt5svg5

    # fixes some issues on X11
    libxfixes-dev
    libxtst-dev

    # KDE Frameworks
    libkf5notifications-dev
    extra-cmake-modules

    # encryption plugin
    gnupg2

    # tests
    xvfb
    openbox
)

apt update
apt -y install "${packages[@]}"

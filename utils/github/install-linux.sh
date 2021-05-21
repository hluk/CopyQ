#!/usr/bin/env bash
# Installs build dependencies.
set -xeuo pipefail

packages=(
    qt5-default

    qtbase5-private-dev
    qtdeclarative5-dev
    qttools5-dev
    qttools5-dev-tools

    libqt5svg5-dev
    libqt5svg5

    # fixes some issues on X11
    libxfixes-dev
    libxtst-dev

    # Wayland support
    extra-cmake-modules
    libqt5waylandclient5-dev
    libwayland-dev
    qtwayland5
    qtwayland5-dev-tools

    # KDE Frameworks
    libkf5notifications-dev
    extra-cmake-modules

    # encryption plugin
    gnupg2

    # tests
    xvfb
    openbox
)

sudo apt-get update
sudo apt-get install "${packages[@]}" "$@"

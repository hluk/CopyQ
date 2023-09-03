#!/usr/bin/env bash
# Installs build dependencies.
set -xeuo pipefail

if [[ $WITH_QT6 == true ]]; then
    qt_packages=(
        libegl-dev

        qt6-base-private-dev
        qt6-base-dev
        qt6-base-dev-tools
        qt6-tools-dev
        qt6-tools-dev-tools
        qt6-l10n-tools
        qt6-declarative-dev

        libqt6svg6-dev
        libqt6svg6

        # Wayland support
        libqt6waylandclient6
        qt6-wayland
        qt6-wayland-dev
        qt6-wayland-dev-tools
    )
else
    qt_packages=(
        qt5-default

        qtbase5-private-dev
        qtdeclarative5-dev
        qttools5-dev
        qttools5-dev-tools

        libqt5x11extras5-dev

        libqt5svg5-dev
        libqt5svg5

        # Wayland support
        libqt5waylandclient5-dev
        qtwayland5
        qtwayland5-dev-tools

        # KDE Frameworks
        libkf5notifications-dev
        extra-cmake-modules
    )
fi

packages=(
    ninja-build

    # fixes some issues on X11
    libxfixes-dev
    libxtst-dev

    # Wayland support
    extra-cmake-modules
    libwayland-dev

    # encryption plugin
    gnupg2

    # tests
    xvfb
    openbox
)

sudo apt-get update
sudo apt-get install "${qt_packages[@]}" "${packages[@]}" "$@"

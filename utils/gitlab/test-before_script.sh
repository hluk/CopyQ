#!/bin/bash
set -ex

packages=(
    libqt5core5a
    libqt5gui5
    libqt5network5
    libqt5script5
    libqt5widgets5
    libqt5svg5
    libqt5xml5
    libqt5test5
    libqt5x11extras5

    libx11-6
    libxtst6
)

apt update

# Runtime libraries
apt -y install "${packages[@]}"

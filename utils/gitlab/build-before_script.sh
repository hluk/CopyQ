#!/bin/bash
set -ex

apt update

apt -y install \
    build-essential \
    cmake \
    extra-cmake-modules \
    git \
    libkf5notifications-dev \
    libqt5svg5 \
    libqt5svg5-dev \
    libqt5waylandclient5-dev \
    libwayland-dev \
    libxfixes-dev \
    libxtst-dev \
    qtbase5-private-dev \
    qtdeclarative5-dev \
    qttools5-dev \
    qttools5-dev-tools \
    qtwayland5 \
    qtwayland5-dev-tools

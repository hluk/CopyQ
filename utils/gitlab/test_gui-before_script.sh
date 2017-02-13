#!/bin/bash
set -ex

packages=(
    # X11 and window manager
    xvfb
    openbox

    # Screenshot utility
    scrot
)

utils/gitlab/test-before_script.sh

apt -y install "${packages[@]}"

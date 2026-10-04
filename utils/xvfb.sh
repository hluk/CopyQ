#!/bin/sh
# Setup virtual X11 environment for development: source utils/xvfb.sh
Xvfb :99 -screen 0 800x600x24 &
sleep 1
export DISPLAY=:99
openbox &

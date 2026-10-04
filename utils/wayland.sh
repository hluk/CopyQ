#!/bin/sh
# Setup virtual X11 environment for development: source utils/wayland.sh
USE_KWIN=${USE_KWIN:-1}
if [[ $USE_KWIN == 1 ]]; then
    kwin_wayland --virtual --socket=copyq-wayland &
else
    weston \
        --backend=headless-backend.so \
        --socket=copyq-wayland \
        --fake-seat \
        --idle-time=0 &
fi
export WAYLAND_DISPLAY=copyq-wayland
export QT_QPA_PLATFORM=wayland

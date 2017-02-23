#!/bin/bash
set -ex

SCREENSHOT_DIR=${SCREENSHOT_DIR:-"screenshots"}
INSTALL_PREFIX=${INSTALL_PREFIX:-"copyq"}
INSTALL_PREFIX=$(readlink -f "$INSTALL_PREFIX")

export DISPLAY=':99.0'
Xvfb :99 -screen 0 640x480x24 &
sleep 5

openbox &
sleep 5

# Start taking screenshots in background.
(
    set +x
    mkdir -p "$SCREENSHOT_DIR"
    while true; do
        i=$((i+1))
        f="$SCREENSHOT_DIR/$i.png"
        sleep 1 &&
            echo "   --- $f ---" &&
            scrot "$f" ||
            break
    done
) &

mkdir -p "$TESTS_LOG_DIR"
export COPYQ_LOG_FILE="$TESTS_LOG_DIR/copyq.log"

"$INSTALL_PREFIX/bin/copyq" tests


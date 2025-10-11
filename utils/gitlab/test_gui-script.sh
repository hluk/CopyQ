#!/bin/bash
set -ex

SCREENSHOT_DIR=${SCREENSHOT_DIR:-"screenshots"}
INSTALL_PREFIX=${INSTALL_PREFIX:-"copyq"}
INSTALL_PREFIX=$(readlink -f "$INSTALL_PREFIX")

export DISPLAY=':99.0'
Xvfb :99 -screen 0 1280x960x24 &
sleep 5

openbox &
sleep 5

mkdir -p "$TESTS_LOG_DIR"
export COPYQ_LOG_FILE="$TESTS_LOG_DIR/copyq.log"

# Start taking screenshots in background.
(
    set +x
    mkdir -p "$SCREENSHOT_DIR"
    while true; do
        i=$((i+1))
        f="$SCREENSHOT_DIR/$i.png"
        sleep 1 &&
            echo "   --- $f ---" &&
            echo "   --- $f ---" >> "$COPYQ_LOG_FILE" &&
            scrot "$f" ||
            break
    done
) &

# Avoid following warning:
#     QtWarning: QStandardPaths: XDG_RUNTIME_DIR not set, defaulting to '/tmp/runtime-root'
export XDG_RUNTIME_DIR='/tmp/runtime-root'
mkdir -p "$XDG_RUNTIME_DIR"
chmod 0700 "$XDG_RUNTIME_DIR"

# Disable encryption tests because exporting GPG key asks for password.
export COPYQ_TESTS_SKIP_ITEMENCRYPT=1

export COPYQ_TESTS_SKIP_DRAG_AND_DROP=1

export COPYQ_TESTS_RERUN_FAILED=1
export COPYQ_TESTS_NO_NETWORK=1
"$INSTALL_PREFIX/bin/copyq" tests

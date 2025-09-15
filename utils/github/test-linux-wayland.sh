#!/bin/bash
# Runs tests for Wayland.
set -xeuo pipefail

# Run only specific tests that are expected to work on Wayland.
default_wayland_tests=(
    commandShowHide
    commandCopy
    commandClipboard
    commandHasClipboardFormat
    clipboardToItem
    itemToClipboard
    avoidStoringPasswords
)

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG
export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.*.warning=true"

export QT_QPA_PLATFORM=wayland

env QT_LOGGING_RULES="" \
    kwin_wayland --virtual --socket=copyq-wayland &
trap "kill $!" QUIT TERM INT HUP EXIT
sleep 10
export WAYLAND_DISPLAY=copyq-wayland

# Smoke test the default session
./copyq --start-server exit

if [[ $# == 0 ]]; then
    ./copyq tests "${default_wayland_tests[@]}"
else
    ./copyq tests "$@"
fi

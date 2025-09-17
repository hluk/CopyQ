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

kwin_wayland --virtual --socket=copyq-wayland &
trap "kill $!" QUIT TERM INT HUP EXIT
export WAYLAND_DISPLAY=copyq-wayland

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG
export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.*.warning=true"

export QT_QPA_PLATFORM=wayland

# Smoke test the default session
for i in {1..5}; do
    echo "Trying to start CopyQ server ($i)"
    if ./copyq --start-server exit; then
        break
    elif [[ $i == 5 ]]; then
        echo "❌ FAILED: Could not start CopyQ server"
        exit 1
    fi
    sleep $((i * 2))
done

# Test handling Unix signals.
script_root="$(dirname "$(readlink -f "$0")")"
"$script_root/test-signals.sh"

if [[ $# == 0 ]]; then
    ./copyq tests "${default_wayland_tests[@]}"
else
    ./copyq tests "$@"
fi

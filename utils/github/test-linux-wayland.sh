#!/bin/bash
# Runs tests for Wayland.
set -xeuo pipefail

# Run only specific tests that are expected to work on Wayland.
default_wayland_tests=(
    testCore:configPath
    testCore:readLog
    testCore:commandShowHide
    testCore:commandCopy
    testCore:commandClipboard
    testCore:commandHasClipboardFormat
    testCore:clipboardToItem
    testCore:itemToClipboard
    testCore:avoidStoringPasswords
    testCore:trayShowHideAction
)

QT_LOGGING_RULES="kwin*.debug=true;kf6.*.debug=true" \
    kwin_wayland --virtual --socket=copyq-wayland 2>&1 > kwin.log &
kwin_pid=$!
kill_kwin() {
    echo '--- KWIN Log ---'
    cat kwin.log || true
    kill $! || true
}
trap kill_kwin QUIT TERM INT HUP EXIT
export WAYLAND_DISPLAY=copyq-wayland

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG
export QT_LOGGING_RULES=${QT_LOGGING_RULES:-"*.debug=true;qt.*.debug=false;qt.*.warning=true"}

export QT_QPA_PLATFORM=wayland

# Smoke test the default session
tries=5
for ((i = 1; i <= tries; i++ )); do
    echo "Trying to start CopyQ server ($i)"
    if "${COPYQ_TESTS_EXECUTABLE:-./copyq}" --start-server exit; then
        break
    elif [[ $i == $tries ]]; then
        echo "❌ FAILED: Could not start CopyQ server"
        exit 1
    fi
    sleep $((i * 2))
done

if [[ $# == 0 ]]; then
    # Test handling Unix signals.
    script_root="$(dirname "$(readlink -f "$0")")"
    "$script_root/test-signals.sh"

    ./copyq-tests "${default_wayland_tests[@]}"
else
    ./copyq-tests "$@"
fi

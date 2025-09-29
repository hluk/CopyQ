#!/bin/bash
# Tests global shortcuts on X11.
set -xeuo pipefail

export COPYQ_SESSION_NAME=__COPYQ_SHORTCUT

source "$(dirname "$0")/test-start-server.sh"
trap "kill $copyq_pid || true" QUIT TERM INT HUP EXIT

./copyq removeTab TEST || true

# register Ctrl+Alt+T to exit CopyQ
./copyq - <<EOF
setCommands(
    [
        {
            name: "Test Command",
            cmd: "copyq tab TEST add TEST",
            globalShortcuts: ["ctrl+alt+t"],
            isGlobalShortcut: true,
        }
    ]
)
EOF
sleep 2

trigger_shortcut() {
    xdotool \
        keydown Control_L \
        keydown Alt_L \
        key t \
        keyup Alt_L \
        keyup Control_L
}

trigger_shortcut

if [[ $(./copyq tab TEST count) == 1 ]]; then
    echo "✅ PASSED: Global shortcut registered: Command executed"
else
    echo "❌ FAILED: Global shortcut registered: Command not executed"
    exit 1
fi

# Unregister the shortcut
./copyq 'setCommands([])'
trigger_shortcut

if [[ $(./copyq tab TEST count) == 1 ]]; then
    echo "✅ PASSED: Global shortcut unregistered: Command not executed"
else
    echo "❌ FAILED: Global shortcut unregistered: Command executed"
    exit 1
fi

#!/bin/bash
# Starts CopyQ server for tests.
# Usage: source test-start-server.sh

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG
export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.*.warning=true"

./copyq &
copyq_pid=$!

# Wait for server to start
for i in {1..3}; do
    echo "Trying to start CopyQ server ($i)"
    if ./copyq 'serverLog("Server started")'; then
        break
    elif [[ $i == 5 ]]; then
        echo "❌ FAILED: Could not start CopyQ server"
        exit 1
    fi
    sleep $((i * 2))
done

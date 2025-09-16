#!/bin/bash
# Tests for handling Unix signals.
set -xeuo pipefail

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG
export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.*.warning=true"
export COPYQ_SESSION_NAME=__COPYQ_SIGTEST

exit_code=0

./copyq &
copyq_pid=$!
sleep 2
# Run simple command to ensure server fully started.
./copyq 'serverLog("Server started")'

sigterm=15
expected_exit_code=$((128 + sigterm))
script_pid=$$
{
    sleep 2
    if ! pkill -$sigterm -f '^\./copyq sleep\(100000\)$'; then
        echo "❌ FAILED: Could not send SIGTERM to the command"
        kill $script_pid $copyq_pid
    fi
} &
if ./copyq 'sleep(100000)'; then
    echo "❌ FAILED: Interrupt sleep: should exit with an error"
else
    actual_exit_code=$?
    if [[ $actual_exit_code == $expected_exit_code ]]; then
        echo "✅ PASSED: Interrupt sleep: exited with an error as expected"
    else
        echo "❌ FAILED: Interrupt sleep: expected exit code $expected_exit_code, got $actual_exit_code"
    fi
fi

./copyq 'sleep(100000)' &
copyq_sleep_pid=$!

./copyq 'while(true){read(9999999);}' &
copyq_loop_pid=$!

trap "kill -9 $copyq_sleep_pid $copyq_loop_pid" TERM INT

sleep 2
echo "⏱️ Sending SIGTERM to server"
kill $copyq_pid

if wait $copyq_sleep_pid; then
    echo "❌ FAILED: Abort sleep: should exit with an error"
    exit_code=1
else
    echo "✅ PASSED: Abort sleep: exited with an error as expected"
fi

if wait $copyq_loop_pid; then
    echo "❌ FAILED: Abort loop: should exit with an error"
    exit_code=1
else
    echo "✅ PASSED: Abort loop: exited with an error as expected"
fi

if wait $copyq_pid; then
    echo "❌ FAILED: Server should exit with a non-zero code"
    exit_code=1
else
    echo "✅ PASSED: Server exited with a non-zero code as expected"
fi

exit $exit_code

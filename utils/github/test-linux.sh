#!/bin/bash
# Runs tests for console and X11.
set -xeuo pipefail

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG
export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.*.warning=true"

export COPYQ_TESTS_EXECUTABLE=${COPYQ_TESTS_EXECUTABLE:-"./copyq"}

# Test command line arguments that don't need GUI.
DISPLAY="" "$COPYQ_TESTS_EXECUTABLE" --help
DISPLAY="" "$COPYQ_TESTS_EXECUTABLE" --version
DISPLAY="" "$COPYQ_TESTS_EXECUTABLE" --info

# Start X11 and window manager.
export DISPLAY=':99.0'
Xvfb :99 -screen 0 1280x960x24 &
sleep 5
openbox &
sleep 8

# Smoke test the default session
"$COPYQ_TESTS_EXECUTABLE" --start-server exit

# Test handling Unix signals.
"$(dirname "$0")/test-signals.sh"

# Test global shortcuts on X11.
"$(dirname "$0")/test-linux-global-shortcuts.sh"

# Run tests.
export COPYQ_TESTS_RERUN_FAILED=1
./copyq-tests "$@"

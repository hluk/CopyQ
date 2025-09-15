#!/bin/bash
# Runs tests for console and X11.
set -xeuo pipefail

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG
export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.*.warning=true"

# Test command line arguments that don't need GUI.
DISPLAY="" ./copyq --help
DISPLAY="" ./copyq --version
DISPLAY="" ./copyq --info

# Start X11 and window manager.
export DISPLAY=':99.0'
Xvfb :99 -screen 0 1280x960x24 &
sleep 5
openbox &
sleep 8

# Smoke test the default session
./copyq --start-server exit

# Run tests.
export COPYQ_TESTS_RERUN_FAILED=1
./copyq tests "$@"

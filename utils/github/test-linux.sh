#!/bin/bash
# Runs tests.
set -xeuo pipefail

# Enable verbose logging.
export COPYQ_LOG_LEVEL=DEBUG

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

# Clean up old configuration.
rm -rf ~/.config/copyq.test

# Run tests.
export COPYQ_TESTS_RERUN_FAILED=0
./copyq tests "$@"

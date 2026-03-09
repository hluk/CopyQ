#!/bin/bash
set -xeuo pipefail

hdiutil attach CopyQ.dmg
ls -Rl /Volumes
app_bundle_path=$(echo /Volumes/copyq-*/CopyQ.app)
executable="$app_bundle_path/Contents/MacOS/CopyQ"

# Test the app before deployment.
"$executable" --help
"$executable" --version
"$executable" --info

# Test paths and features.
ls "$("$executable" info plugins)/"
ls "$("$executable" info themes)/"
ls "$("$executable" info translations)/"
test "$("$executable" info has-global-shortcuts)" -eq "1"

# Disable animations for tests
defaults write -g NSAutomaticWindowAnimationsEnabled -bool false
defaults write -g NSWindowResizeTime -float 0.001

# Run tests (retry once on error).
export COPYQ_TESTS_RERUN_FAILED=1
export COPYQ_TESTS_SKIP_COMMAND_EDIT=1
export COPYQ_TESTS_SKIP_CONFIG_MOVE=1
export COPYQ_TESTS_SKIP_DRAG_AND_DROP=1
export COPYQ_TESTS_SKIP_SLOW_CLIPBOARD=1
export COPYQ_TESTS_EXECUTABLE="$executable"
./copyq-tests

# Verify the bundle is self-contained by checking linked libraries.
echo "--- Checking bundle dependencies ---"
otool -L "$executable"
otool -L "$app_bundle_path/Contents/PlugIns/"*/*.dylib 2>/dev/null || true
otool -L "$app_bundle_path/Contents/PlugIns/copyq/"* 2>/dev/null || true
otool -L "$app_bundle_path/Contents/Frameworks/"Qt*.framework/Versions/*/Qt* 2>/dev/null || true

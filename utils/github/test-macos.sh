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

# Uninstall local Qt to make sure we only use libraries from the bundle
brew remove --ignore-dependencies --force \
    qt@6 copyq/kde/kf6-knotifications copyq/kde/kf6-kstatusnotifieritem freetype

# Ensure the app works after uninstalling system dependencies
(
    export LD_LIBRARY_PATH=""
    export DYLD_LIBRARY_PATH=""
    "$executable" --start-server '
        info();
        print(plugins.itemtags.tags());
        print(plugins.itemsync.tabPaths);
        exit();
    '
)

# Print dependencies to let us further make sure that we don't depend on local libraries
otool -L "$executable"
otool -L "$app_bundle_path/Contents/PlugIns/"*/*.dylib
otool -L "$app_bundle_path/Contents/PlugIns/copyq/"*
otool -L "$app_bundle_path/Contents/Frameworks/"Qt*.framework/Versions/*/Qt*

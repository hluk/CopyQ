#!/bin/bash
# Creates macOS bundle.
set -xeuo pipefail

cpack

app_bundle_path="CopyQ.app"
executable="${PWD}/${app_bundle_path}/Contents/MacOS/CopyQ"

ls -Rl "$app_bundle_path"

# Test the app before deployment.
"$executable" --help
"$executable" --version
"$executable" --info

# Test paths and features.
ls "$("$executable" info plugins)/"
ls "$("$executable" info themes)/"
ls "$("$executable" info translations)/"
test "$("$executable" info has-global-shortcuts)" -eq "1"

# Uninstall local Qt to make sure we only use libraries from the bundle
brew remove --ignore-dependencies --force \
    qt@6 copyq/kde/kf6-knotifications copyq/kde/kf6-kstatusnotifieritem freetype

# Disable animations for tests
defaults write -g NSAutomaticWindowAnimationsEnabled -bool false
defaults write -g NSWindowResizeTime -float 0.001

(
    export LD_LIBRARY_PATH=""

    # Run tests (retry once on error).
    export COPYQ_TESTS_SKIP_COMMAND_EDIT=1
    export COPYQ_TESTS_SKIP_CONFIG_MOVE=1
    export COPYQ_TESTS_RERUN_FAILED=1
    "$executable" tests
)

# Print dependencies to let us further make sure that we don't depend on local libraries
otool -L "$executable"
otool -L "$app_bundle_path/Contents/PlugIns/"*/*.dylib
otool -L "$app_bundle_path/Contents/PlugIns/copyq/"*
otool -L "$app_bundle_path/Contents/Frameworks/"Qt*.framework/Versions/*/Qt*

mv copyq-*.dmg CopyQ.dmg

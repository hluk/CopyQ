#!/bin/bash
# Creates macOS bundle.
set -xeuo pipefail

app_bundle_path="CopyQ.app"

executable="${PWD}/${app_bundle_path}/Contents/MacOS/CopyQ"
plugins=("$app_bundle_path/Contents/PlugIns/copyq/"*.so)
qt_bin="$(brew --prefix qt@5)/bin"

rm -rf "$app_bundle_path/Contents/PlugIns/"{platforminputcontexts,printsupport,qmltooling}

"$qt_bin/macdeployqt" "$app_bundle_path" -dmg -verbose=2 -always-overwrite -no-plugins \
    "${plugins[@]/#/-executable=}"

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
    qt@5 copyq/kde/kf5-knotifications freetype

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
otool -L "$app_bundle_path/Contents/Frameworks/"Qt*.framework/Versions/5/Qt*

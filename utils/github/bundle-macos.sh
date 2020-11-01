#!/bin/bash
# Creates macOS bundle.
set -xeuo pipefail

cpack

app_bundle_path="$(echo _CPack_Packages/Darwin/DragNDrop/copyq-*/CopyQ.app)"
executable="${PWD}/${app_bundle_path}/Contents/MacOS/CopyQ"

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
remove=$(brew list --formula)
brew remove --force --ignore-dependencies $remove

(
    export PATH=""
    export LD_LIBRARY_PATH=""

    # Run tests (retry once on error).
    export COPYQ_TESTS_SKIP_COMMAND_EDIT=1
    export COPYQ_TESTS_SKIP_CONFIG_MOVE=1
    export COPYQ_TESTS_RERUN_FAILED=1
    "$executable" tests
)

# Print dependencies to let us further make sure that we don't depend on local libraries
otool -L "$executable"
otool -L "$app_bundle_path/Contents/PlugIns/copyq/"*
otool -L "$app_bundle_path/Contents/Frameworks/"Qt*.framework/Versions/5/Qt*

mv copyq-*.dmg CopyQ.dmg

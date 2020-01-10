#!/bin/bash
# Build with Travis CI.

set -exuo pipefail

mkdir -p build
cd build

cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt5)" \
    -DWITH_TESTS=ON \
    ..

cmake --build . --target all

cpack

app_bundle_path="_CPack_Packages/Darwin/DragNDrop/copyq-*/CopyQ.app"
executable="$(ls ${PWD}/${app_bundle_path}/Contents/MacOS/CopyQ)"

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
brew uninstall --ignore-dependencies --force qt5

# Run tests (retry once on error).
export COPYQ_TESTS_SKIP_COMMAND_EDIT=1
export COPYQ_TESTS_SKIP_CONFIG_MOVE=1
export COPYQ_TESTS_RERUN_FAILED=1
"$executable" tests

# Print dependencies to let us further make sure that we don't depend on local libraries
otool -L $executable
otool -L ${app_bundle_path}/Contents/PlugIns/copyq/*
otool -L ${app_bundle_path}/Contents/Frameworks/Qt*.framework/Versions/5/Qt*

mv copyq-*.dmg CopyQ.dmg

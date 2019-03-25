#!/bin/bash
# Build with Travis CI.

set -exuo pipefail

root=$PWD
mkdir -p build
cd build

cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_TESTS=ON \
    ..

cmake --build . --target all --parallel

cpack

executable="$(ls ${PWD}/_CPack_Packages/Darwin/DragNDrop/copyq-*/CopyQ.app/Contents/MacOS/CopyQ)"

# Test the app before deployment.
"$executable" --help
"$executable" --version
"$executable" --info

# Test paths and features.
ls "$("$executable" info plugins)/"
ls "$("$executable" info themes)/"
ls "$("$executable" info translations)/"
test "$("$executable" info has-global-shortcuts)" -eq "1"

# Run tests (retry once on error).
export COPYQ_TESTS_SKIP_COMMAND_EDIT=1
export COPYQ_TESTS_SKIP_CONFIG_MOVE=1
"$executable" tests ||
    "$executable" tests

# Create "CopyQ.dmg".
cp -a copyq-*.dmg CopyQ.dmg

cd "$root"

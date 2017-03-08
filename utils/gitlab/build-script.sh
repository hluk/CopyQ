#!/bin/bash
set -ex

BUILD_DIR=${BUILD_DIR:-"build"}
INSTALL_PREFIX=${INSTALL_PREFIX:-"copyq"}
INSTALL_PREFIX=$(readlink -f "$INSTALL_PREFIX")

cmake_args=(
    -DWITH_TESTS=TRUE
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
    ..
)

mkdir -p "$BUILD_DIR"

(
    cd "$BUILD_DIR"
    cmake "${cmake_args[@]}" ..
    cmake --build . --target install
)

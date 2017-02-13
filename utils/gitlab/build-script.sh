#!/bin/bash
set -ex

BUILD_DIR=${BUILD_DIR:-"build"}
INSTALL_PREFIX=${INSTALL_PREFIX:-"copyq"}
INSTALL_PREFIX=$(readlink -f "$INSTALL_PREFIX")

cmake_args=(
    -DWITH_TESTS=TRUE
    -DWITH_QT5=TRUE
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
    -DCOPYQ_ITEMSYNC_UPDATE_INTERVAL_MS=1000
    ..
)

mkdir -p "$BUILD_DIR"

(
    cd "$BUILD_DIR"
    cmake "${cmake_args[@]}" ..
    cmake --build . --target install
)

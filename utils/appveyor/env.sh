#!/usr/bin/bash
set -exu

export PATH=$QTDIR/bin:$PATH

if [[ -n ${MINGW_PATH:-} ]]; then
    export PATH=$MINGW_PATH/bin:$PATH
fi

export BUILD_PATH=$APPVEYOR_BUILD_FOLDER/build
export CMAKE_PREFIX_PATH="$QTDIR/lib/cmake"

if [[ $WITH_NATIVE_NOTIFICATIONS == ON ]]; then
    export KF5_FULLVER=$KF5_VERSION.$KF5_PATCH
    export INSTALL_PREFIX=$APPVEYOR_BUILD_FOLDER/usr/kf-$KF5_FULLVER-$CMAKE_GENERATOR_ARCH
    export DEPENDENCY_PATH=$APPVEYOR_BUILD_FOLDER/dependencies
    export DOWNLOADS_PATH=$APPVEYOR_BUILD_FOLDER/downloads
    export DEPENDENCY_BUILD_PATH=$DEPENDENCY_PATH/build

    export CMAKE_PREFIX=$INSTALL_PREFIX/lib/cmake
    export CMAKE_PREFIX_PATH="$CMAKE_PREFIX:$INSTALL_PREFIX/share/ECM/cmake":$CMAKE_PREFIX_PATH

    export SNORETOAST_BASE_URL=https://invent.kde.org/libraries/snoretoast/-/archive/v$SNORETOAST_VERSION
    export KF5_BASE_URL=https://download.kde.org/stable/frameworks/$KF5_VERSION

    mkdir -p "$INSTALL_PREFIX"
    mkdir -p "$DEPENDENCY_PATH"
    mkdir -p "$DOWNLOADS_PATH"
fi

APP_VERSION=$(git describe --tags --always HEAD)
export APP_VERSION
export APP=copyq-$APP_VERSION

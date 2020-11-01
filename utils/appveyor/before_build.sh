#!/usr/bin/bash
set -exo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

build=$APPVEYOR_BUILD_FOLDER/utils/appveyor/kf5_build.sh

export PATH=$PATH:$INSTALL_PREFIX/bin

"$build" snoretoast "v$SNORETOAST_VERSION" "$SNORETOAST_BASE_URL"
"$build" extra-cmake-modules
"$build" kconfig "" "" "-DKCONFIG_USE_GUI=OFF"
"$build" kwindowsystem
"$build" kcoreaddons
"$build" knotifications

# Create and upload dependencies zip file.
7z a "$APP-dependencies.zip" -r "$INSTALL_PREFIX"
appveyor PushArtifact "$APP-dependencies.zip" -DeploymentName "CopyQ Dependencies"

cmake -B"$BUILD_PATH" -DCMAKE_BUILD_TYPE=Release \
    -G "$CMAKE_GENERATOR" -A "$CMAKE_GENERATOR_ARCH" \
    -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
    -DCMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION=. \
    -DWITH_TESTS=ON

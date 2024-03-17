#!/usr/bin/bash
set -exo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

languages=$APPVEYOR_BUILD_FOLDER/Shared/Languages
mkdir -p "$languages"
# This can be removed when InnoSetup >= 6.2.3 is available.
curl --location --silent --show-error --fail-with-body \
    --remote-name-all --output-dir "$languages" \
    https://github.com/jrsoftware/issrc/raw/main/Files/Languages/Korean.isl
grep -q LanguageName "$languages/Korean.isl"

if [[ $WITH_NATIVE_NOTIFICATIONS == ON ]]; then
    build=$APPVEYOR_BUILD_FOLDER/utils/appveyor/kf5_build.sh

    export PATH=$PATH:$INSTALL_PREFIX/bin

    "$build" snoretoast "v$SNORETOAST_VERSION" "$SNORETOAST_BASE_URL"
    "$build" extra-cmake-modules
    "$build" kconfig "" "" "-DKCONFIG_USE_DBUS=OFF" "-DKCONFIG_USE_GUI=OFF"
    "$build" kwindowsystem
    "$build" kcoreaddons
    "$build" knotifications

    # Create and upload dependencies zip file.
    7z a "$APP-dependencies.zip" -r "$INSTALL_PREFIX"
    appveyor PushArtifact "$APP-dependencies.zip" -DeploymentName "CopyQ Dependencies"
fi

if [[ -n ${CMAKE_GENERATOR_ARCH:-} ]]; then
    cmake_args=(-A "$CMAKE_GENERATOR_ARCH")
else
    cmake_args=()
fi

cmake -B"$BUILD_PATH" -DCMAKE_BUILD_TYPE=Release \
    -G "$CMAKE_GENERATOR" "${cmake_args[@]}" \
    -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
    -DCMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION=. \
    -DWITH_NATIVE_NOTIFICATIONS="$WITH_NATIVE_NOTIFICATIONS" \
    -DWITH_TESTS=ON

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

build=$APPVEYOR_BUILD_FOLDER/utils/appveyor/kf_build.sh
export PATH=$PATH:$INSTALL_PREFIX/bin

if [[ $WITH_QCA_ENCRYPTION == ON ]]; then
    URL_PATH="$QCA_VERSION/qca-$QCA_VERSION" \
        "$build" qca "$QCA_VERSION" "https://download.kde.org/stable/qca" \
            -DBUILD_WITH_QT6="$WITH_QT6" \
            -DBUILD_TESTS=OFF \
            -DBUILD_TOOLS=OFF \
            -DBUILD_PLUGINS=ossl &
    qca_pid=$!

    URL_PATH="$QTKEYCHAIN_VERSION" \
    DOWNLOAD_SUFFIX=tar.gz \
        "$build" qtkeychain "$QTKEYCHAIN_VERSION" "https://github.com/frankosterfeld/qtkeychain/archive/refs/tags" \
            -DBUILD_WITH_QT6="$WITH_QT6" \
            -DBUILD_TRANSLATIONS=OFF \
            -DBUILD_TEST_APPLICATION=OFF &
    qtkeychain_pid=$!
fi

if [[ $WITH_NATIVE_NOTIFICATIONS == ON ]]; then
    DOWNLOAD_SUFFIX=zip \
        "$build" snoretoast "v$SNORETOAST_VERSION" "$SNORETOAST_BASE_URL" &
    snoretoast_pid=$!

    "$build" extra-cmake-modules &
    ecm_pid=$!
fi

# Wait for Group A (no inter-dependencies)
if [[ $WITH_QCA_ENCRYPTION == ON ]]; then
    wait "$qca_pid" "$qtkeychain_pid"
fi

if [[ $WITH_NATIVE_NOTIFICATIONS == ON ]]; then
    wait "$snoretoast_pid" "$ecm_pid"

    # Group B: both depend on ECM only
    "$build" kconfig "" "" "-DKCONFIG_USE_DBUS=OFF" "-DKCONFIG_USE_GUI=OFF" &
    kconfig_pid=$!
    "$build" kwindowsystem &
    kwindowsystem_pid=$!
    wait "$kconfig_pid" "$kwindowsystem_pid"

    # Group C: depend on kconfig + kwindowsystem
    "$build" knotifications &
    knotifications_pid=$!
    "$build" kstatusnotifieritem &
    kstatusnotifieritem_pid=$!
    wait "$knotifications_pid" "$kstatusnotifieritem_pid"
fi

# Create and upload dependencies zip file.
7z a "$APP-dependencies.zip" -r "$INSTALL_PREFIX"
appveyor PushArtifact "$APP-dependencies.zip" -DeploymentName "CopyQ Dependencies"

if [[ -n ${CMAKE_GENERATOR_ARCH:-} ]]; then
    cmake_args=(-A "$CMAKE_GENERATOR_ARCH")
else
    cmake_args=()
fi

cmake -B"$BUILD_PATH" -DCMAKE_BUILD_TYPE=Release \
    -G "$CMAKE_GENERATOR" "${cmake_args[@]}" \
    -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
    -DCMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION=. \
    -DCMAKE_CXX_FLAGS="-MP" \
    -DWITH_NATIVE_NOTIFICATIONS="$WITH_NATIVE_NOTIFICATIONS" \
    -DWITH_QCA_ENCRYPTION="$WITH_QCA_ENCRYPTION" \
    -DWITH_KEYCHAIN="$WITH_QCA_ENCRYPTION" \
    -DWITH_QT6="$WITH_QT6" \
    -DWITH_TESTS=ON

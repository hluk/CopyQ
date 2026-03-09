#!/usr/bin/env bash
# Builds KDE framework dependencies for CopyQ on macOS (GitHub Actions).
# Uses Ninja. Expects Qt from install-qt-action.
#
# Expected environment variables:
#   GITHUB_WORKSPACE, CMAKE_PREFIX_PATH,
#   KF_VERSION, KF_PATCH, KF_BRANCH,
#   QCA_VERSION, QTKEYCHAIN_VERSION
set -exo pipefail

: "${GITHUB_WORKSPACE:?}"
: "${CMAKE_PREFIX_PATH:?}"
: "${KF_VERSION:?}"
: "${KF_PATCH:?}"
: "${KF_BRANCH:?}"
: "${QCA_VERSION:?}"
: "${QTKEYCHAIN_VERSION:?}"

KF_FULLVER=${KF_VERSION}.${KF_PATCH}
KF_BASE_URL="https://download.kde.org/${KF_BRANCH}/frameworks/${KF_VERSION}"

INSTALL_PREFIX=${DEPS_PREFIX:-$GITHUB_WORKSPACE/deps/install}
DOWNLOADS_DIR=$GITHUB_WORKSPACE/deps/downloads
BUILD_DIR=$GITHUB_WORKSPACE/deps/build
PATCH_DIR=$GITHUB_WORKSPACE/utils/patches

mkdir -p "$INSTALL_PREFIX" "$DOWNLOADS_DIR" "$BUILD_DIR"
export PATH=$INSTALL_PREFIX/bin:$PATH

# Generic dependency builder.
#   build_dep NAME VERSION BASE_URL [CMAKE_ARGS...]
# Override URL path:  BUILD_DEP_URL_PATH=... build_dep ...
# Override archive:   BUILD_DEP_SUFFIX=... build_dep ...
build_dep() {
    local name=$1
    local version=$2
    local base_url=$3
    shift 3
    local extra_cmake_args=("$@")

    local suffix=${BUILD_DEP_SUFFIX:-tar.xz}
    local url_path=${BUILD_DEP_URL_PATH:-"$name-$version"}
    local url="$base_url/$url_path.$suffix"
    local pkg="$DOWNLOADS_DIR/$name-$version.$suffix"

    # Download with retry.
    curl -sSLo "$pkg" --fail-with-body --retry 5 --retry-all-errors "$url"

    # Extract.
    (cd "$DOWNLOADS_DIR" && cmake -E tar xf "$pkg")

    # Apply patches if any exist.
    local src_dir="$DOWNLOADS_DIR/$name-$version"
    if [[ -d "$PATCH_DIR/$name" ]]; then
        local patch
        for patch in "$PATCH_DIR/$name"/*.patch; do
            [[ -f "$patch" ]] && (cd "$src_dir" && patch -p1 < "$patch")
        done
    fi

    # Configure + build.
    cmake -B "$BUILD_DIR/$name" -S "$src_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DBUILD_TESTING=OFF \
        "${extra_cmake_args[@]}"

    cmake --build "$BUILD_DIR/$name" --parallel
}

# Install a previously built dependency.
#   install_dep NAME
install_dep() {
    cmake --install "$BUILD_DIR/$1"
}

# Group A: No inter-dependencies, build in parallel.
BUILD_DEP_URL_PATH="${QCA_VERSION}/qca-${QCA_VERSION}" \
    build_dep qca "$QCA_VERSION" "https://download.kde.org/stable/qca" \
        -DBUILD_WITH_QT6=ON \
        -DBUILD_TESTS=OFF \
        -DBUILD_TOOLS=OFF \
        -DBUILD_PLUGINS=ossl &
qca_pid=$!

BUILD_DEP_URL_PATH="$QTKEYCHAIN_VERSION" BUILD_DEP_SUFFIX=tar.gz \
    build_dep qtkeychain "$QTKEYCHAIN_VERSION" \
        "https://github.com/frankosterfeld/qtkeychain/archive/refs/tags" \
        -DBUILD_WITH_QT6=ON \
        -DBUILD_TRANSLATIONS=OFF \
        -DBUILD_TEST_APPLICATION=OFF &
qtkeychain_pid=$!

build_dep extra-cmake-modules "$KF_FULLVER" "$KF_BASE_URL" &
ecm_pid=$!

wait "$qca_pid" "$qtkeychain_pid" "$ecm_pid"
install_dep qca
install_dep qtkeychain
install_dep extra-cmake-modules

# Group B: Depend on ECM.
build_dep kconfig "$KF_FULLVER" "$KF_BASE_URL" \
    -DKCONFIG_USE_DBUS=OFF \
    -DKCONFIG_USE_GUI=OFF &
kconfig_pid=$!

build_dep kwindowsystem "$KF_FULLVER" "$KF_BASE_URL" &
kwindowsystem_pid=$!

wait "$kconfig_pid" "$kwindowsystem_pid"
install_dep kconfig
install_dep kwindowsystem

# Group C: Depend on kconfig + kwindowsystem.
build_dep knotifications "$KF_FULLVER" "$KF_BASE_URL" \
    -DUSE_DBUS=OFF &
knotifications_pid=$!

build_dep kstatusnotifieritem "$KF_FULLVER" "$KF_BASE_URL" \
    -DUSE_DBUS=OFF &
kstatusnotifieritem_pid=$!

wait "$knotifications_pid" "$kstatusnotifieritem_pid"
install_dep knotifications
install_dep kstatusnotifieritem

echo "All dependencies built successfully."

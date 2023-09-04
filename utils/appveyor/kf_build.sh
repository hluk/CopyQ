#!/usr/bin/bash
set -exo pipefail

name=$1
version=${2:-$KF_FULLVER}
base_url=${3:-$KF_BASE_URL}

if [[ $# -gt 3 ]]; then
    shift 3
    extra_cmake_args=("$@")
else
    extra_cmake_args=()
fi

url=$base_url/$name-$version
patch_dir=$APPVEYOR_BUILD_FOLDER/utils/appveyor/patches/$name
state_old=$INSTALL_PREFIX/$name-state
state_new=$DEPENDENCY_PATH/$name-state-new

shopt -s nullglob

function get_new_state() {
    echo "URL=$url"
    echo "CMAKE_GENERATOR=$CMAKE_GENERATOR"
    echo "CMAKE_GENERATOR_ARCH=$CMAKE_GENERATOR_ARCH"
    echo "CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
    echo "EXTRA_CMAKE_ARGS=${extra_cmake_args[*]}"
    echo | md5sum "$patch_dir"/*.patch
}

function get_source() {
    suffix=$1
    pkg=$DOWNLOADS_PATH/$name-$version.$suffix

    for retry in $(seq 5); do
        curl -sSL -o "$pkg" "$url.$suffix"
        if cmake -E tar xf "$pkg"; then
            return 0
        fi
        if [[ $retry -gt 0 ]]; then
            sleep $((retry * 15))
        fi
    done
    return 1
}

# Omit rebuilding if already cached.
get_new_state > "$state_new"
if diff "$state_old" "$state_new"; then
    exit
fi

(
    cd "$DEPENDENCY_PATH"

    if [[ "$base_url" == "$KF_BASE_URL" ]]; then
        get_source tar.xz
    else
        get_source zip
    fi

    cd "$name-$version"

    for patch in "$patch_dir"/*.patch; do
        patch -p1 < "$patch"
    done

    cmake -B"$DEPENDENCY_BUILD_PATH/$name" -DCMAKE_BUILD_TYPE=Release \
        -G "$CMAKE_GENERATOR" -A "$CMAKE_GENERATOR_ARCH" \
        -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DBUILD_TESTING=OFF \
        "${extra_cmake_args[@]}"

    cmake --build "$DEPENDENCY_BUILD_PATH/$name" --config Release --target install
)

mv "$state_new" "$state_old"

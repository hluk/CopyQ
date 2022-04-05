#!/usr/bin/bash
set -exo pipefail

name=$1
version=${2:-$KF5_FULLVER}
base_url=${3:-$KF5_BASE_URL}

if [[ $# -gt 3 ]]; then
    shift 3
    extra_cmake_args=("$@")
else
    extra_cmake_args=()
fi

pkg=$DOWNLOADS_PATH/$name-$version.zip
url=$base_url/$name-$version.zip
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

# Omit rebuilding if already cached.
get_new_state > "$state_new"
if diff "$state_old" "$state_new"; then
    exit
fi

(
    cd "$DEPENDENCY_PATH"
    # Retry downloading package.
    for retry in $(seq 5); do
        if cmake -E tar xf "$pkg" --format=zip; then
            break
        fi
        rm -f "$pkg"
        if [[ $retry -gt 0 ]]; then
            sleep $((retry * 15))
        fi
        curl -L -o "$pkg" "$url"
    done
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

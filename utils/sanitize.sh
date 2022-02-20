#!/bin/bash
set -e

sanitizers="address,leak,bounds,bool,enum,null,undefined"
export CXX_FLAGS="$CXX_FLAGS -fno-omit-frame-pointer -mllvm -asan-use-private-alias=1"

build_dir=build-sanitize
utils_dir=$(dirname "$(readlink -f "$0")")
src_dir=$utils_dir/..
steps=(
    clean
    cmake
    ninja
    run
    )

if [ -n "$CLANG_ROOT" ]; then
    export PATH=$CLANG_ROOT:$PATH
fi
CXX=${CXX:-clang++}

export ASAN_SYMBOLIZER_PATH=${ASAN_SYMBOLIZER_PATH:-$(which llvm-symbolizer)}
export ASAN_OPTIONS=${ASAN_OPTIONS:-"detect_leaks=1 detect_stack_use_after_return=1 print_stats=1 use_odr_indicator=1"}

echo "=== Using sanitizers (directory \"$build_dir\") ==="

step=2
if [ -d "$build_dir" ]; then
    echo "Select starting point:"
    select step in "${steps[@]}"; do step=$REPLY; break; done
fi

has_step () {
    [[ $step -le $1 ]]
}

has_step "${#steps[*]}"

if has_step 1; then
    rm -r "$build_dir" || exit 1
fi

mkdir -p "$build_dir"
cd "$build_dir"

if has_step 2; then
    cmake \
        -DCMAKE_INSTALL_PREFIX="$PWD/install" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DCMAKE_CXX_FLAGS="$CXX_FLAGS -fsanitize=$sanitizers" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_GENERATOR=Ninja \
        "$src_dir"
fi

if has_step 3; then
    ninja install
fi

if has_step 4; then
    ./copyq -s test1 "$@"
fi


#!/bin/bash
set -e

# file:///usr/share/doc/clang/html/UsersManual.html#controlling-code-generation
sanitize=${sanitize:-memory}
sanitize_other="bounds,bool,enum,null"
compiler_flags=""

build_dir=build-clang-$sanitize
utils_dir=$(dirname $(readlink -f "$0"))
src_dir=$utils_dir/..
steps=(
    clean
    cmake
    make
    run
    )

if [ -n "$CLANG_ROOT" ]; then
    export PATH=$CLANG_ROOT:$PATH
fi
CXX=${CXX:-clang++}

if [ "$sanitize" == "address" ]; then
    sanitize_other="$sanitize_other,address-full,init-order"

    export ASAN_SYMBOLIZER_PATH=${ASAN_SYMBOLIZER_PATH:-$(which llvm-symbolizer)}
    export ASAN_OPTIONS=${ASAN_OPTIONS:-"detect_leaks=1 detect_stack_use_after_return=1 print_stats=1"}
elif [ "$sanitize" == "memory" ]; then
    compiler_flags="$compiler_flags -fsanitize-memory-track-origins"

    # from https://llvm.org/svn/llvm-project/compiler-rt/trunk/lib/msan/msan.cc
    export MSAN_SYMBOLIZER_PATH=${MSAN_SYMBOLIZER_PATH:-$(which llvm-symbolizer)}
    export MSAN_OPTIONS=${MSAN_OPTIONS:-"keep_going=1 halt_on_error=0 report_umrs=0"}
elif [ "$sanitize" == "thread" ]; then
    echo
fi

echo "=== Using $sanitize sanitizer (directory \"$build_dir\") ==="

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
        -DCMAKE_CXX_FLAGS="-fsanitize=$sanitize,$sanitize_other $compiler_flags -fno-omit-frame-pointer" \
        -DCMAKE_BUILD_TYPE=Debug \
        "$src_dir"
fi

if has_step 3; then
    make -j4 install
fi

if has_step 4; then
    ./copyq -s test1 "$@"
fi


#!/bin/bash
# Build with coverage flags, run tests and highlight executed lines.
# usage: [build=0] [cover=0] [show=0] ./coverage.sh [base_filename]
src_dir=$(readlink -f .)
build_dir="build-coverage"
build=${build:-1}
cover=${cover:-1}
show=${show:-1}
file=${1:-scriptable}

set -e

if [ "$1" = "-h" -o "$1" = "--help" ]; then
    echo "usage: [build=0] [cover=0] [show=0] $0 [base_filename]"
    exit
fi

if [ "$build" = "1" ]; then
    rm -rf "$build_dir"
    mkdir "$build_dir"

    (
    cd "$build_dir"

    qmake \
        QMAKE_CXXFLAGS="-fprofile-arcs -ftest-coverage" \
        QMAKE_LFLAGS="-fprofile-arcs -ftest-coverage -lgcov" \
        CONFIG+=debug \
        "$src_dir"

    make -j4
    )
fi

if [ "$cover" = "1" ]; then
    (
    cd "$build_dir"

    "$src_dir/tests/test.sh" ./copyq

    cd src
    find -name '*.gcno' -exec gcov {} \; > /dev/null
    )
fi

if [ "$show" = "1" ]; then
    IFS=""
    cat "$build_dir/src/$file.cpp.gcov" |
    while read line; do
        c=""
        case $line in
            *"     -: "*)
                c="\e[0;36m"
                ;;
            *" #####: "*)
                c="\e[0;33m"
                ;;
            *)
                c="\e[1;36m"
                ;;
        esac
        printf "$c%s\e[0m\n" "$line"
    done | less -R
fi


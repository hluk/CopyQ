#!/bin/bash
# Generate coverage report for Coveralls after running tests with Travis CI.

# Exclude system and 3rd party files.
exclude_files=(
    qxt
    /usr
    plugins/itemfakevim/fakevim
    src/gui/fix_icon_id.h
)

# Exclude generated files.
exclude_regexs=(
    'build/.*'
    '.*/moc_.*'
    '.*\.moc$'
    '.*_automoc\..*'
    '.*/ui_.*'
    '.*/qrc_.*'
    '.*CMake.*'
    '.*/tests/.*'
    '.*/src/gui/add_icons.h'
)

# Generate coverage report only with GCC.
if [ "$CC" == "gcc" ]; then
    arguments=()

    for file in "${exclude_files[@]}"; do
        arguments+=(--exclude "$file")
    done

    for regex in "${exclude_regexs[@]}"; do
        arguments+=(--exclude-pattern "$regex")
    done

    coveralls \
        --build-root "build" \
        --gcov "$GCOV" \
        "${arguments[@]}"
fi

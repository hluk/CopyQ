#!/bin/bash
# Generate coverage report for Coveralls after running tests with Travis CI.

# Exclude system and 3rd party files.
exclude_files=(
    qxt
    src/tests
    /usr
    plugins/itemfakevim/fakevim
)

# Exclude generated files.
exclude_regexs=(
    '.*/moc_.*'
    '.*\.moc$'
    '.*/ui_.*'
    '.*/qrc_.*'
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
        "${arguments[@]}"
fi

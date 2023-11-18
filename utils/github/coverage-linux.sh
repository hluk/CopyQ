#!/bin/bash
# Generates and uploads coverage report for Coveralls after running tests.
set -xeuo pipefail

build_root=$1

# Exclude system and 3rd party files.
exclude_files=(
    qxt
    /usr
    plugins/itemfakevim/fakevim
    src/gui/fix_icon_id.h
)

# Exclude generated files.
exclude_regexs=(
    "$build_root/.*"
    'build/.*'
    '.*/moc_.*'
    '.*\.moc$'
    '.*_automoc\..*'
    '.*/ui_.*'
    '.*/qrc_.*'
    '.*CMake.*'
)

arguments=()

for file in "${exclude_files[@]}"; do
    arguments+=(--exclude "$file")
done

for regex in "${exclude_regexs[@]}"; do
    arguments+=(--exclude-pattern "$regex")
done

export PATH="$HOME/.local/bin:$PATH"
pip install --user 'urllib3[secure]' cpp-coveralls

# Looks like coveralls only supports build directory inside the source code
# directory.
ln -s "$build_root" build

coveralls \
    --follow-symlinks \
    --build-root "build" \
    --gcov gcov \
    --gcov-options '\-lp' \
    "${arguments[@]}"

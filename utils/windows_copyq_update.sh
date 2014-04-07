#!/bin/bash
# Script to update CopyQ installation on Windows.
src=${1:-"../build-copyq-Qt_4_8_5-Release"}
dst=${2:-"/c/dev/copyq"}

set -e

update() {
    set -e
    for file in "$@"; do
        cp -uv -- "$file" "$dst/$file"
    done
}

mkdir -p "$dst/"{plugins,themes}

update "README.md" "AUTHORS" "HACKING"

cd shared
update themes/*.ini
cd -

cd "$src"
update copyq.exe copyq.com plugins/*.dll
cd -



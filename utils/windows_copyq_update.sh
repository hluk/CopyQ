#!/bin/bash
# Script to update CopyQ installation on Windows.
build=${1:-"../build-copyq-Qt_4_8_5-Release"}
dst=${2:-"/c/dev/copyq"}
src=${3:-$PWD}

set -e

update() {
    set -e
    mkdir -p "${@: -1}"
    cp -uv -- "$@"
}

update "$src"/{README.md,AUTHORS,HACKING,LICENSE} "$dst"
update "$src"/shared/themes/*.{css,ini} "$dst/themes"
update "$build"/copyq.exe "$dst"
update "$build"/plugins/*.dll "$dst/plugins"
update "$build"/src/*.qm "$dst/translations"

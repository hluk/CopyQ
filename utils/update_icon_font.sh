#!/bin/bash
# Updates icon font.
set -e

font_awesome_src=${1:-$HOME/apps/Font-Awesome}
icons_yml=$font_awesome_src/src/icons.yml
font=$font_awesome_src/fonts/fontawesome-webfont.ttf

self=$(readlink -f "$0")
script_name=$(basename "$self")
utils_dir=$(dirname "$self")

header=$utils_dir/../src/gui/add_icons.h

cp -v "$font" "$utils_dir/../src/images/fontawesome-webfont.ttf"

(
    echo "// This file is generated with \"$script_name\" from FontAwesome's \"icons.yml\" and"
    echo "// contains list of method calls for IconSelectDialog."

    grep 'unicode:' "$icons_yml" | sed -e 's/.*\s/addIcon(0x/' -e 's/$/);/'
) > "$header"

echo "Header file \"$header\" updated."

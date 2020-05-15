#!/bin/bash
set -e

make_icons()
{
    input_icon=$1
    output_basename=$2

    for extent in 16 22 24 32 48 64 128 256 512 1024; do
        size="${extent}x${extent}"
        output="${output_basename}_$size.png"
        echo "Converting $input_icon -> $output"
        convert -background transparent -density 600 "$input_icon" \
            -resize "$size" "$output"
    done
}

cd src/images
make_icons icon.svg icon

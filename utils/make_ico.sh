#!/bin/bash
image_dir=src/images
out=${1:-src/images/icon.ico}

args=(
    -background transparent
    ${image_dir}/icon_16x16.png
    \( ${image_dir}/icon_22x22.png -resize 20x20 \)
    ${image_dir}/icon_24x24.png
    ${image_dir}/icon_32x32.png
    \( ${image_dir}/icon_48x48.png -resize 40x40 \)
    ${image_dir}/icon_48x48.png
    ${image_dir}/icon_64x64.png
    \( ${image_dir}/icon_128x128.png -resize 96x96 \)
    ${image_dir}/icon_128x128.png
    ${image_dir}/icon_256x256.png
    "$out"
    )

exec convert "${args[@]}"


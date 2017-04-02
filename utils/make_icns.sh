#!/bin/bash
set -ex

image_dir=src/images
tmp_dir=icon.iconset

out=${image_dir}/icon.icns

rm -rf "${tmp_dir}"
mkdir -p "${tmp_dir}"

convert_img () {
    size=$1
    highdpi=$2

    target_img=${tmp_dir}/icon_${size}x${size}
    if [[ -n "$highdpi" ]]; then
        target_img="${target_img}@${highdpi}x"
        size=$(expr $size \* $highdpi)
    fi
    target_img="${target_img}.png"

    source_img=${image_dir}/icon_${size}x${size}.png

    cp "$source_img" "$target_img"
}

convert_img 16
convert_img 16 2
convert_img 32
convert_img 32 2
convert_img 128
convert_img 128 2
convert_img 256
convert_img 256 2
convert_img 512
convert_img 512 2

iconutil --convert icns --output $out "${tmp_dir}"
rm -rf "${tmp_dir}"

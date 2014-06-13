#!/bin/bash

image_dir=src/images
tmp_dir=icon.iconset

img1=${image_dir}/icon.svg
img2=${image_dir}/logo.svg
out=${image_dir}/icon.icns

if ! which rsvg-convert &>/dev/null
then
    echo "This script requires rsvg-convert"
    echo "On OS X, you can install with"
    echo " > brew install librsvg"
    exit 1
fi

rm -rf ${tmp_dir}
mkdir -p ${tmp_dir}

convert_img () {
    img=$1
    size=$2
    size_name=$2
    highdpi=$3
    outfile=${tmp_dir}/icon_${size_name}x${size_name}
    [[ -n "$highdpi" ]] \
        && outfile="${outfile}@${highdpi}x" \
        && size=$(expr $size \* $highdpi)
    rsvg-convert -w $size -h $size -f png -o "${outfile}.png" "$img"
    # ImageMagick doesn't get colors right..
    # convert -background transparent $img -resize ${size}x${size} "${outfile}.png"
}

convert_img $img1 16
convert_img $img1 16 2
convert_img $img2 32
convert_img $img2 32 2
convert_img $img2 128
convert_img $img2 128 2
convert_img $img2 256
convert_img $img2 256 2
convert_img $img2 512
convert_img $img2 512 2

iconutil --convert icns --output $out ${tmp_dir}
rm -rf ${tmp_dir}

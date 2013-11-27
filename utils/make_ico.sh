#!/bin/bash
img1=src/images/icon.svg
img2=src/images/logo.svg
out=${1:-src/images/icon.ico}

args=(
    -background transparent
    \( "$img1" -resize 16x16 \)
    -density 600
    \( "$img1" -resize 32x32 \)
    \( "$img2" -resize 48x48 \)
    \( "$img2" -resize 64x64 \)
    \( "$img2" -resize 96x96 \)
    \( "$img2" -resize 128x128 \)
    "$out"
    )

exec convert "${args[@]}"


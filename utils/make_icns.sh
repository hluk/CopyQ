#!/bin/bash

img1=src/images/icon.svg
img2=src/images/logo.svg
out=src/images/icon.icns

rm -rf icon.iconset
mkdir -p icon.iconset

convert -background transparent $img1 -resize 16x16 icon.iconset/icon_16x16.png
convert -background transparent $img1 -resize 32x32 icon.iconset/icon_16x16@2x.png
convert -background transparent $img2 -resize 32x32 icon.iconset/icon_32x32.png
convert -background transparent $img2 -resize 64x64 icon.iconset/icon_32x32@2x.png
convert -background transparent $img2 -resize 128x128 icon.iconset/icon_128x128.png
convert -background transparent $img2 -resize 256x256 icon.iconset/icon_128x128@2x.png
convert -background transparent $img2 -resize 256x256 icon.iconset/icon_256x256.png
convert -background transparent $img2 -resize 512x512 icon.iconset/icon_256x256@2x.png
convert -background transparent $img2 -resize 512x512 icon.iconset/icon_512x512.png
convert -background transparent $img2 -resize 1024x1024 icon.iconset/icon_512x512@2x.png

iconutil --convert icns --output $out icon.iconset 
rm -rf icon.iconset

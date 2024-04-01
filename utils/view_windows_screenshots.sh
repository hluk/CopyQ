#!/bin/bash
set -euo pipefail

image_viewer=${IMAGE_VIEWER:-eog}
url="https://ci.appveyor.com/api/projects/hluk/copyq/artifacts/copyq-screenshots.zip?all=true&$*"

if [[ $# == 0 ]]; then
    echo "Usage: $0 {tag=v8.0.0|branch=master|pr=true}"
    exit 1
fi

dir=$(mktemp --directory)
clean() {
    rm -rf -- "$dir"
}
trap clean QUIT TERM INT HUP EXIT

curl -LsSf --output "$dir/copyq-screenshots.zip" "$url"
7z e -o"$dir" "$dir/copyq-screenshots.zip"
"$image_viewer" "$dir"

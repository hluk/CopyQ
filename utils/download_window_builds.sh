#!/bin/bash
set -euo pipefail
version=$1
base=https://ci.appveyor.com/api/projects/hluk/copyq/artifacts
urls=(
    "$base/copyq-$version.zip?tag=v$version"
    "$base/copyq-$version-setup.exe?tag=v$version"
)
for url in "${urls[@]}"; do
    echo "Downloading: $url"
    curl -LO --fail-with-body "$url"
done
echo "DONE"

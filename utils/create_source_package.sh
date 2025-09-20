#!/bin/bash
version=$1
prefix=Copyq-$version
out=${2:-"$prefix.tar.gz"}
out=$(readlink -f "$out")

script=$(readlink -f "$0")
source="$(dirname "$(dirname "$script")")"

set -e

die () {
    echo "ERROR: $*"
    exit 1
}

git -C "$source" archive --format=tar.gz --prefix="$prefix/" --output="$out" "v$version" ||
    die "First arguments must be existing version (tag v<VERSION> must exist in repository)"

echo "Created source package for version $version: $out"

size=$(stat --format="%s" "$out")
hash=$(md5sum "$out" | cut -d' ' -f 1)
echo " $hash $size $out"

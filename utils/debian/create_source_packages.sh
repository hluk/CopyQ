#!/bin/bash
set -e

distros=(
    bionic
    focal
    hirsute
    impish
    jammy
)

sed -i 's/quilt/native/' 'debian/source/format'
echo 9 > 'debian/compat'
rm -f debian/watch

for distro in "${distros[@]}"; do
    ./utils/debian/update_changelog.sh "$distro"
    debuild -S -d
done

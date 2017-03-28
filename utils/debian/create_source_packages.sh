#!/bin/bash
set -e

distros=(
    trusty
    xenial
    yakkety
    zesty
)

for distro in "${distros[@]}"; do
    ./utils/debian/update_changelog.sh "$distro"
    debuild -S
done

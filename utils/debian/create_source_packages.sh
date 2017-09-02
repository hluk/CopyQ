#!/bin/bash
set -e

distros=(
    trusty
    xenial
    zesty
    artful
)

for distro in "${distros[@]}"; do
    ./utils/debian/update_changelog.sh "$distro"
    debuild -S
done

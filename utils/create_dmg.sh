#!/bin/bash

# Creates a .dmg file for CopyQ

base_path=$(cd $(dirname $0)/../; pwd)

version=$(${base_path}/copyq.app/Contents/MacOS/copyq --version | grep 'CopyQ' | sed 's/.*v\([0-9.]*\) .*/\1/')
stripped_version=$(echo "${version}" | sed 's/\.//g')

if ! python -c 'import Quartz' &>/dev/null || ! which dmgbuild &>/dev/null
then
    echo "$(basename $0) requires dmgbuild and pyobjc to be installed. These can" 1>&2
    echo "be installed with 'pip install dmgbuild pyobjc'" 1>&2
    exit 1
fi

dmg="${base_path}/copyq${stripped_version}.dmg"
name="CopyQ ${version}"
echo "Creating ${dmg} for \"${name}\""

dmgbuild -Dapp_path=${base_path}/copyq.app -s ${base_path}/shared/dmg_settings.py "${name}" ${dmg}

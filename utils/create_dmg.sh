#!/bin/bash

set -e -x

# Creates a .dmg file for CopyQ

script_path=$(cd $(dirname $0); pwd)
app_path=${1:-$(pwd)/CopyQ.app}

if ! python -c 'from Quartz import *' &>/dev/null || ! which dmgbuild &>/dev/null
then
    echo "$(basename $0) requires dmgbuild and pyobjc-framework-Quartz to be installed." 1>&2
    echo "These can be installed with 'pip install dmgbuild pyobjc-framework-Quartz'" 1>&2
    exit 1
fi

binary="${app_path}/Contents/MacOS/copyq"
settings="${script_path}/../shared/dmg_settings.py"

! [[ -f "${settings}" ]] && echo "${settings} is not a file" && exit 1
! [[ -d "${app_path}" ]] && echo "${app_path} is not a directory" && exit 1
! [[ -x "${binary}" ]] && echo "unable to execute ${binary}" && exit 1

version=$(${binary} --version | grep 'CopyQ' | sed 's/.*v\([0-9.]*\) .*/\1/')

dmg="${app_path}/../CopyQ-${version}.dmg"
name="CopyQ ${version}"
echo "Creating ${dmg} for \"${name}\""

dmgbuild -Dapp_path="${app_path}" -s "${settings}" "${name}" "${dmg}"

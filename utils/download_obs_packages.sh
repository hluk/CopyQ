#!/bin/bash
# Download packages from openSUSE Build Service.
version=$1

rpm_version="4.1"
base_url="http://download.opensuse.org/repositories/home:/"
user=${2:-"lukho"}
project=${3:-"copyq"}
url=$base_url$user:/$project

urls=(
    "$url/xUbuntu_13.10/i386/${project}_${version}_i386.deb"
    "$url/xUbuntu_13.10/amd64/${project}_${version}_amd64.deb"
    "$url/xUbuntu_13.04/i386/${project}_${version}_i386.deb"
    "$url/xUbuntu_13.04/amd64/${project}_${version}_amd64.deb"
    "$url/xUbuntu_12.10/i386/${project}_${version}_i386.deb"
    "$url/xUbuntu_12.10/amd64/${project}_${version}_amd64.deb"
    "$url/xUbuntu_12.04/i386/${project}_${version}_i386.deb"
    "$url/xUbuntu_12.04/amd64/${project}_${version}_amd64.deb"
    "$url/openSUSE_13.1/x86_64/${project}-${version}-${rpm_version}.x86_64.rpm"
    "$url/openSUSE_13.1/i586/${project}-${version}-${rpm_version}.i586.rpm"
    "$url/Fedora_19/x86_64/${project}-${version}-${rpm_version}.x86_64.rpm"
    "$url/Debian_7.0/i386/${project}_${version}_i386.deb"
    "$url/Debian_7.0/amd64/${project}_${version}_amd64.deb"
)

die () {
    echo "ERROR: $*"
    exit 1
}

get_name () {
    sed 's#.*/'"$project"'/x\?\([^/]*\)/.*/'"$project"'[_-]\([^_-]*\)[_-]\(.*\)$#'"$project"'_\2_\1_\3#' <<< "$1"
}

if [ -z "$version" ]; then
    die "First argument must be version package version!"
fi

for url in "${urls[@]}"; do
    name=$(get_name "$url")
    wget -nc "$url" -O "$name"
done


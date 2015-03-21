#!/bin/bash
# Download packages from openSUSE Build Service.
version=$1

rpm_version=$2
base_url="http://download.opensuse.org/repositories/home:/"
user=${3:-"lukho"}
project=${4:-"copyq"}
url=$base_url$user:/$project

urls=(
    "$url/xUbuntu_14.10/i386/${project}_${version}_i386.deb"
    "$url/xUbuntu_14.10/amd64/${project}_${version}_amd64.deb"
    "$url/xUbuntu_14.04/i386/${project}_${version}_i386.deb"
    "$url/xUbuntu_14.04/amd64/${project}_${version}_amd64.deb"
    "$url/xUbuntu_12.04/i386/${project}_${version}_i386.deb"
    "$url/xUbuntu_12.04/amd64/${project}_${version}_amd64.deb"
    "$url/openSUSE_Tumbleweed/x86_64/${project}-${version}-${rpm_version}.x86_64.rpm"
    "$url/Fedora_21/x86_64/${project}-${version}-${rpm_version}.x86_64.rpm"
    "$url/Fedora_20/x86_64/${project}-${version}-${rpm_version}.x86_64.rpm"
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
    wget -c "$url" -O "$name" || exit 2
done


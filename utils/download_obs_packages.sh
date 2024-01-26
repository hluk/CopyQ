#!/bin/bash
# Download packages from openSUSE Build Service.
version=$1
rpm_version=$2
user=${3:-"lukho"}
project=${4:-"copyq"}

base_url="https://download.opensuse.org/repositories/home:/"
url=$base_url$user:/$project

xdeb="-1_amd64.deb"
xdeb_armhf="-1_armhf.deb"
xdeb_arm64="-1_arm64.deb"
xrpm=".x86_64.rpm"

pkg="${project}_${version}"
pkg_deb="amd64/${pkg}${xdeb}"
pkg_deb_armhf="armhf/${pkg}${xdeb_armhf}"
pkg_deb_arm64="arm64/${pkg}${xdeb_arm64}"
pkg_rpm="x86_64/${project}-${version}-${rpm_version}${xrpm}"

failed=""

die () {
    echo "ERROR: $*"
    exit 1
}

fetch_package () {
    name=$1
    package_url=$2
    wget -c "$package_url" -O "$name" || failed="$failed\n$package_url"
}

if [ -z "$version" ]; then
    die "First argument must be version package version!"
fi

fetch_package "${pkg}_openSUSE_Tumbleweed${xrpm}" "$url/openSUSE_Tumbleweed/${pkg_rpm}"
fetch_package "${pkg}_openSUSE_Leap_15.4${xrpm}"  "$url/15.4/x86_64/${project}-${version}-lp154.${rpm_version}${xrpm}"
fetch_package "${pkg}_Debian_10${xdeb}"           "$url/Debian_10/${pkg_deb}"
fetch_package "${pkg}_Debian_11${xdeb}"           "$url/Debian_11/${pkg_deb}"
fetch_package "${pkg}_Debian_12${xdeb}"           "$url/Debian_12/${pkg_deb}"
fetch_package "${pkg}_Raspbian_12${xdeb_armhf}"   "$url/Raspbian_12/${pkg_deb_armhf}"
fetch_package "${pkg}_Raspbian_12${xdeb_arm64}"   "$url/Raspbian_12/${pkg_deb_arm64}"

if [ -n "$failed" ]; then
    echo -e "Failed packages:$failed"
    exit 2
fi


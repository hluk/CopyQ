#!/bin/bash
# Download packages from openSUSE Build Service.
version=$1
rpm_version=$2
user=${3:-"lukho"}
project=${4:-"copyq"}

base_url="http://download.opensuse.org/repositories/home:/"
url=$base_url$user:/$project

xdeb="_amd64.deb"
xdeb_i386="_i386.deb"
xrpm=".x86_64.rpm"

pkg="${project}_${version}"
pkg_deb="amd64/${pkg}${xdeb}"
pkg_deb_i386="i386/${pkg}${xdeb_i386}"
pkg_rpm="x86_64/${project}-${version}-${rpm_version}${xrpm}"

packages=(
    "${pkg}_Ubuntu_15.10${xdeb_i386}   $url/xUbuntu_15.10/${pkg_deb_i386}"
    "${pkg}_Ubuntu_15.10${xdeb}        $url/xUbuntu_15.10/${pkg_deb}"
    "${pkg}_Ubuntu_15.04${xdeb_i386}   $url/xUbuntu_15.04/${pkg_deb_i386}"
    "${pkg}_Ubuntu_15.04${xdeb}        $url/xUbuntu_15.04/${pkg_deb}"
    "${pkg}_Ubuntu_14.10${xdeb_i386}   $url/xUbuntu_14.10/${pkg_deb_i386}"
    "${pkg}_Ubuntu_14.10${xdeb}        $url/xUbuntu_14.10/${pkg_deb}"
    "${pkg}_Ubuntu_14.04${xdeb_i386}   $url/xUbuntu_14.04/${pkg_deb_i386}"
    "${pkg}_Ubuntu_14.04${xdeb}        $url/xUbuntu_14.04/${pkg_deb}"
    "${pkg}_Ubuntu_12.04${xdeb_i386}   $url/xUbuntu_12.04/${pkg_deb_i386}"
    "${pkg}_Ubuntu_12.04${xdeb}        $url/xUbuntu_12.04/${pkg_deb}"
    "${pkg}_openSUSE_Tumbleweed${xrpm} $url/openSUSE_Tumbleweed/${pkg_rpm}"
    "${pkg}_Fedora_22${xrpm}           $url/Fedora_22/${pkg_rpm}"
    "${pkg}_Fedora_23${xrpm}           $url/Fedora_23/${pkg_rpm}"
    "${pkg}_Debian_8.0${xdeb_i386}     $url/Debian_8.0/${pkg_deb_i386}"
    "${pkg}_Debian_8.0${xdeb}          $url/Debian_8.0/${pkg_deb}"
    "${pkg}_Debian_7.0${xdeb_i386}     $url/Debian_7.0/${pkg_deb_i386}"
    "${pkg}_Debian_7.0${xdeb}          $url/Debian_7.0/${pkg_deb}"
)

die () {
    echo "ERROR: $*"
    exit 1
}

if [ -z "$version" ]; then
    die "First argument must be version package version!"
fi

for package in "${packages[@]}"; do
    name=$(sed 's/\s.*//' <<< "$package")
    url=$(sed 's/.*\s//' <<< "$package")
    wget -c "$url" -O "$name" || exit 2
done


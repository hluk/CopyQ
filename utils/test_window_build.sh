#!/bin/bash
# Downloads and tests Windows build from Appveyor.
# Usage:
#    utils/test_window_build.sh [BRANCH=master [VERSION=$(git describe origin/BRANCH)]]
set -ex

branch=${1:-master}
version=${2:-$(git describe "origin/$branch")}
project=hluk/copyq
tmp_dir=/tmp/copyq-test
archive="copyq-$version.zip"
url="https://ci.appveyor.com/api/projects/$project/artifacts/$archive?branch=$branch"

if [[ ! -f "$archive" ]]; then
  curl -o "$archive" "$url"
fi

unzip "$archive" -d "$tmp_dir"
wine "$tmp_dir/copyq-$version/copyq.exe"

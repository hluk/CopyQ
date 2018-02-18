#!/bin/bash
set -e

distro=${1:-xenial}
version="$(git describe|sed 's/^v//;s/-/./;s/-/~/')~$distro"

git checkout HEAD debian/control debian/rules
sed -i 's/debhelper .*,/debhelper (>= 9),/' 'debian/control'
sed -i 's/Standards-Version:.*/Standards-Version: 3.9.7/' 'debian/control'

git checkout HEAD debian/changelog

dch \
  -M \
  -v "$version" \
  -D "$distro" "from git commit $(git rev-parse HEAD)"

git diff

echo "To upload source code for $version run:"
echo "    debuild -S && dput ppa:hluk/copyq-beta ../copyq_${version}_source.changes"

#!/bin/bash
set -e

distro=${1:-xenial}
version="$(git describe|sed 's/^v//;s/-/./;s/-/~/')~$distro"

if [ "$distro" == "trusty" ]; then
    cp debian/control{-qt4,}
    cp debian/rules{-qt4,}
else
    git checkout HEAD debian/control debian/rules
fi

git checkout HEAD debian/changelog

dch \
  -M \
  -v "$version" \
  -D "$distro" "from git commit $(git rev-parse HEAD)"

git diff

echo "To upload source code for $version run:"
echo "    debuild -S && dput ppa:hluk/copyq-beta ../copyq_${version}_source.changes"

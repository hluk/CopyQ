#!/usr/bin/bash
set -exuo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

export PATH="$GPGPATH":$Destination:$PATH
mkdir ~/.gnupg
chmod go-rwx ~/.gnupg
gpg --version

cp -v \
    "$BUILD_PATH/${BUILD_SUB_DIR:-}/copyq-tests.exe" \
    "$QTDIR/bin/Qt6Test.dll" \
    "$Destination"

export COPYQ_PLUGINS=$BUILD_PATH/src/${BUILD_SUB_DIR:-}/itemtests.dll
"$Destination/copyq-tests.exe"

gpgconf --kill all

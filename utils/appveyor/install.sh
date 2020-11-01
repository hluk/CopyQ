#!/usr/bin/bash
set -exo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

ls "$QTDIR/bin"
ls "$QTDIR/lib/cmake"

ls "$OPENSSL_PATH"
ls "$OPENSSL_PATH/$LIBCRYPTO"
ls "$OPENSSL_PATH/$LIBSSL"

"$QTDIR/bin/qmake" --version

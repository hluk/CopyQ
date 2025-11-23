#!/usr/bin/bash
set -exo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

ls "$QTDIR/bin"
ls "$QTDIR/lib/cmake"

ls /c/OpenSSL*-Win64/bin
ls "$LIBCRYPTO"
ls "$LIBSSL"
if [[ $WITH_NATIVE_NOTIFICATIONS == ON ]]; then
    ls "$LIBCRYPTO_FOR_QCA"
    ls "$LIBSSL_FOR_QCA"
fi

"$QTDIR/bin/qmake" --version

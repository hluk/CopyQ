#!/usr/bin/bash
set -exuo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

export PATH="$GPGPATH":$Destination:$PATH
mkdir ~/.gnupg
chmod go-rwx ~/.gnupg
gpg --version

"$Executable" tests

gpgconf --kill all

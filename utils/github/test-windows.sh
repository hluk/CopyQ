#!/usr/bin/bash
# Runs CopyQ tests on Windows CI (GitHub Actions).
#
# Expected environment variables:
#   COPYQ_PLUGINS  - path to test plugin DLL
#   GPGPATH        - path to GPG binaries
#
# Run from the build directory containing copyq-tests.exe.
set -exuo pipefail

export PATH="$GPGPATH:$PATH"
mkdir -p ~/.gnupg
chmod go-rwx ~/.gnupg
gpg --version

./copyq-tests.exe

#!/bin/bash
set -ex

INSTALL_PREFIX=${INSTALL_PREFIX:-"copyq"}
INSTALL_PREFIX=$(readlink -f "$INSTALL_PREFIX")

"$INSTALL_PREFIX/bin/copyq" help
"$INSTALL_PREFIX/bin/copyq" version
"$INSTALL_PREFIX/bin/copyq" info

#!/usr/bin/env bash
# Installs build dependencies.
set -xeuo pipefail

brew uninstall cmake

# Install Homebrew: https://brew.sh/
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"

if [[ $BUILDNAME == 'macOS old' ]]; then
	HOMEBREW_DEVELOPER=1 brew install ./qtbase--*.bottle.tar.gz
fi

brew install qt@6 cmake
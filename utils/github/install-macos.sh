#!/usr/bin/env bash
# Installs build dependencies.
set -xeuo pipefail

qt_package=$1

# Create repository for Homebrew.
(
    cd utils/github/homebrew/
    git init .
    git add .
    git commit -m "Initial"
)

# workaround for symlink issue
rm -rf /usr/local/bin/2to3

# Install Homebrew: https://brew.sh/
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"

if [[ $qt_package == qt ]]; then
    brew install qt
else
    brew tap copyq/kde utils/github/homebrew/
    brew install "$qt_package" copyq/kde/kf5-knotifications
fi

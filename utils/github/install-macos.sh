#!/usr/bin/env bash
# Installs build dependencies.
set -xeuo pipefail

# Create repository for Homebrew.
(
    cd utils/github/homebrew/
    git config --global user.email "noreply@github.com"
    git config --global user.name "GitHub Actions"
    git init .
    git add .
    git commit -m "Initial"
)

# workaround for symlink issues
rm -rf \
    /usr/local/bin/2to3* \
    /usr/local/bin/idle3* \
    /usr/local/bin/pydoc3* \
    /usr/local/bin/python3* \
    /usr/local/bin/python3-config*

brew tap copyq/kde utils/github/homebrew/

brew install qt@6 qca qtkeychain

# Install KDE frameworks (skip build if restored from cache).
if brew list --versions kf6-kstatusnotifieritem >/dev/null 2>&1; then
    echo "KDE frameworks restored from cache, re-linking..."
    brew link --overwrite \
        extra-cmake-modules \
        kf6-kconfig \
        kf6-kwindowsystem \
        kf6-knotifications \
        kf6-kstatusnotifieritem
else
    brew install --verbose \
        copyq/kde/kf6-knotifications \
        copyq/kde/kf6-kstatusnotifieritem
fi

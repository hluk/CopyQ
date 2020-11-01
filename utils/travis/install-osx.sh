#!/usr/bin/env bash

set -e -x

# Installing Homebrew packages is handled by a Travis CI addon:
# https://docs.travis-ci.com/user/installing-dependencies/#installing-packages-on-macos

# KDE Frameworks: https://invent.kde.org/packaging/homebrew-kde
#brew untap kde-mac/kde || true
#brew tap kde-mac/kde https://invent.kde.org/packaging/homebrew-kde.git --force-auto-update
#"$(brew --repo kde-mac/kde)/tools/do-caveats.sh"
#
#brew install qt5 kde-mac/kde/kf5-knotifications

#!/usr/bin/env bash

set -e -x

# KDE Frameworks: https://invent.kde.org/packaging/homebrew-kde
brew untap kde-mac/kde || true
brew tap kde-mac/kde https://invent.kde.org/packaging/homebrew-kde.git --force-auto-update
"$(brew --repo kde-mac/kde)/tools/do-caveats.sh"

brew install qt5 kde-mac/kde/kf5-knotifications

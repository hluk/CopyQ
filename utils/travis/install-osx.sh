#!/usr/bin/env bash

set -e -x

WHEELHOUSE="$HOME/.wheelhouse"

function pip_with_cache() {
    # Pre-cache all dependencies as wheels in the cache dir
    pip wheel \
        --use-wheel \
        --wheel-dir="$WHEELHOUSE" \
        --find-links="file://$WHEELHOUSE" \
        "$@"

    # Install from the wheelhouse
    sudo pip install \
        --use-wheel \
        --find-links="file://$WHEELHOUSE" \
        "$@"
}

brew install qt5
sudo pip install wheel
pip_with_cache \
    cpp-coveralls \
    dmgbuild \
    'pyobjc-framework-Quartz==3.0.1' \
    'pyobjc-framework-Cocoa==3.0.1'

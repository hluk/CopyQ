#!/bin/bash
# Automatically re-build the documentation whenever it changes.
#
# Requires sphinx-autobuild script:
#
#     pip install --user sphinx-autobuild
#     export PATH="$HOME/.local/bin:$PATH"
#
exec sphinx-autobuild \
    --ignore '.*' \
    --ignore "*.swp" \
    --ignore "*~" \
    . _build/html

#!/bin/bash
set -xeuo pipefail

hdiutil attach CopyQ*.dmg
ls -Rl /Volumes
app_bundle_path=$(echo /Volumes/copyq-*/CopyQ.app)
executable="$app_bundle_path/Contents/MacOS/CopyQ"

# Test the app before deployment.
"$executable" --help
"$executable" --version
"$executable" --info

# Test paths and features.
ls "$("$executable" info plugins)/"
ls "$("$executable" info themes)/"
ls "$("$executable" info translations)/"
test "$("$executable" info has-global-shortcuts)" -eq "1"

# Disable animations for tests
defaults write -g NSAutomaticWindowAnimationsEnabled -bool false
defaults write -g NSWindowResizeTime -float 0.001

# Run tests (retry once on error).
export COPYQ_TESTS_RERUN_FAILED=1
export COPYQ_TESTS_SKIP_COMMAND_EDIT=1
export COPYQ_TESTS_SKIP_CONFIG_MOVE=1
export COPYQ_TESTS_SKIP_DRAG_AND_DROP=1
export COPYQ_TESTS_SKIP_SLOW_CLIPBOARD=1
export COPYQ_TESTS_EXECUTABLE="$executable"
./copyq-tests

# Verify the bundle is self-contained: every @rpath reference resolves to a
# library that is actually present in the Frameworks directory.
echo '--- Checking bundle for unresolved @rpath references ---'
frameworks_dir="$app_bundle_path/Contents/Frameworks"
unresolved=$(
    find "$app_bundle_path" -type f \( -name '*.dylib' -o -perm /111 \) -print0 |
    xargs -0 otool -L 2>/dev/null |
    grep -o '@rpath/[^ ]*' |
    sort -u |
    while read -r ref; do
        rel=${ref#@rpath/}
        if [[ ! -e "$frameworks_dir/$rel" ]]; then
            echo "$ref"
        fi
    done || true
)
if [[ -n "$unresolved" ]]; then
    echo 'ERROR: Unresolved @rpath references in bundle:'
    echo "$unresolved"
    exit 1
fi
echo 'OK: All @rpath references resolve within the bundle.'

# Verify minimum macOS deployment target is at most 13.0.
echo '--- Checking minimum macOS version ---'
min_version=$(otool -l "$executable" | awk '/LC_BUILD_VERSION/{found=1} found && /minos/{print $2; exit}')
if [[ -z "$min_version" ]]; then
    # Fallback: try LC_VERSION_MIN_MACOSX
    min_version=$(otool -l "$executable" | awk '/LC_VERSION_MIN_MACOSX/{found=1} found && /version/{print $2; exit}')
fi
if [[ -z "$min_version" ]]; then
    echo 'ERROR: Could not determine minimum macOS version from binary.'
    exit 1
fi
echo "Minimum macOS version: $min_version"
major=${min_version%%.*}
if [[ "$major" -gt 13 ]]; then
    echo "ERROR: Minimum macOS version $min_version exceeds 13.x"
    exit 1
fi
echo 'OK: Minimum macOS version is acceptable.'

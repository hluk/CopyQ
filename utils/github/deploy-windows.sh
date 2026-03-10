#!/usr/bin/bash
# Assembles a deployable CopyQ directory on Windows (GitHub Actions).
# Copies dependencies, runs windeployqt, prepares for testing and packaging.
#
# Based on the proven Appveyor after_build.sh workflow.
#
# Expected environment variables:
#   GITHUB_WORKSPACE, BUILD_DIR, APP_VERSION,
#   QT_ROOT_DIR, DEPS_PREFIX, OPENSSL_ROOT_DIR
set -exuo pipefail

: "${GITHUB_WORKSPACE:?}"
: "${BUILD_DIR:?}"
: "${APP_VERSION:?}"
: "${QT_ROOT_DIR:?}"
: "${DEPS_PREFIX:?}"
: "${OPENSSL_ROOT_DIR:?}"

APP="copyq-${APP_VERSION}"
DEST="${GITHUB_WORKSPACE}/${APP}"

mkdir -p "$DEST"

cmake --install "$BUILD_DIR" --config Release --prefix "$DEST" --verbose

# Copy QCA and QtKeychain libraries.
mkdir -p "$DEST/crypto"
cp -v "$DEPS_PREFIX/bin/qca-qt6.dll" "$DEST"
cp -v "$DEPS_PREFIX/lib/qca-qt6/crypto/qca-ossl.dll" "$DEST/crypto/"
cp -v "$DEPS_PREFIX/bin/qt6keychain.dll" "$DEST"

# Workaround for windeployqt: https://github.com/frankosterfeld/qtkeychain/issues/246
cp -v "$DEPS_PREFIX/bin/qt6keychain.dll" "$QT_ROOT_DIR/bin/"

crypto_libraries=(
    "$DEST/qca-qt6.dll"
    "$DEST/qt6keychain.dll"
)

# Copy KDE framework and SnoreToast libraries.
cp -v "$DEPS_PREFIX/bin/KF6"*.dll "$DEST"
cp -v "$DEPS_PREFIX/bin/snoretoast.exe" "$DEST"
kf_libraries=(
    "$DEST/KF6ConfigCore.dll"
    "$DEST/KF6Notifications.dll"
)

# Copy project assets.
cp -v "$GITHUB_WORKSPACE/AUTHORS" "$DEST"
cp -v "$GITHUB_WORKSPACE/LICENSE" "$DEST"
cp -v "$GITHUB_WORKSPACE/README.md" "$DEST"

mkdir -p "$DEST/themes"
cp -v "$GITHUB_WORKSPACE/shared/themes/"* "$DEST/themes"

mkdir -p "$DEST/translations"
cp -v "$BUILD_DIR/src/"*.qm "$DEST/translations"

mkdir -p "$DEST/plugins"
cp -v "$BUILD_DIR/plugins/"*.dll "$DEST/plugins"

# Copy OpenSSL libraries.
cp -v "$OPENSSL_ROOT_DIR"/bin/libcrypto-*.dll "$DEST"
cp -v "$OPENSSL_ROOT_DIR"/bin/libssl-*.dll "$DEST"

# Run windeployqt to gather Qt runtime dependencies.
"$QT_ROOT_DIR/bin/windeployqt" --help
"$QT_ROOT_DIR/bin/windeployqt" \
    --no-system-d3d-compiler \
    --no-system-dxc-compiler \
    --no-compiler-runtime \
    --no-opengl-sw \
    --no-quick \
    --skip-plugin-types qmltooling,generic,networkinformation \
    "${kf_libraries[@]}" \
    "${crypto_libraries[@]}" \
    "$DEST/copyq.exe"

# Clean up workaround file to avoid polluting the cached Qt installation.
rm -f "$QT_ROOT_DIR/bin/qt6keychain.dll"

# Copy test binary and its Qt dependency into the deployed directory
# so tests run against the fully deployed application layout.
cp -v "$BUILD_DIR/copyq-tests.exe" "$DEST/"
cp -v "$QT_ROOT_DIR/bin/Qt6Test.dll" "$DEST/"
cp -v "$BUILD_DIR/src/itemtests.dll" "$DEST/"

# Remove system-installed OpenSSL to verify bundled libs are used.
rm -vf /c/Windows/System32/libcrypto-*
rm -vf /c/Windows/System32/libssl-*
rm -vf /c/Windows/SysWOW64/libcrypto-*
rm -vf /c/Windows/SysWOW64/libssl-*

# Verify the deployed binary works with only bundled libraries.
OldPath=$PATH
export PATH="$DEST"
"$DEST/copyq.exe" --help
"$DEST/copyq.exe" --version
"$DEST/copyq.exe" --info
export PATH=$OldPath

echo "Deploy complete: $DEST"

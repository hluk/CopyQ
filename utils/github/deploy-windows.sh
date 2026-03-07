#!/usr/bin/bash
# Assembles a deployable CopyQ directory on Windows (GitHub Actions).
# Copies dependencies, runs windeployqt, prepares for testing and packaging.
#
# Expected environment variables:
#   GITHUB_WORKSPACE, BUILD_DIR, APP_VERSION,
#   QT_ROOT_DIR, DEPS_PREFIX, OPENSSL_ROOT_DIR
set -exo pipefail

: "${GITHUB_WORKSPACE:?}"
: "${BUILD_DIR:?}"
: "${APP_VERSION:?}"
: "${QT_ROOT_DIR:?}"
: "${DEPS_PREFIX:?}"
: "${OPENSSL_ROOT_DIR:?}"

APP="copyq-${APP_VERSION}"
DEST="${GITHUB_WORKSPACE}/${APP}"

mkdir -p "$DEST"

# Install CopyQ into the destination.
cmake --install "$BUILD_DIR" --config Release --prefix "$DEST" --verbose

# Copy QCA and QtKeychain libraries.
mkdir -p "$DEST/crypto"
cp -v "$DEPS_PREFIX/bin/qca-qt6.dll" "$DEST/"
cp -v "$DEPS_PREFIX/lib/qca-qt6/crypto/qca-ossl.dll" "$DEST/crypto/"
cp -v "$DEPS_PREFIX/bin/qt6keychain.dll" "$DEST/"

# Workaround for windeployqt: https://github.com/frankosterfeld/qtkeychain/issues/246
cp -v "$DEPS_PREFIX/bin/qt6keychain.dll" "$QT_ROOT_DIR/bin/"

# Copy KDE framework and SnoreToast libraries.
cp -v "$DEPS_PREFIX/bin/KF6"*.dll "$DEST/"
cp -v "$DEPS_PREFIX/bin/snoretoast.exe" "$DEST/"

# Copy project assets.
cp -v "$GITHUB_WORKSPACE/AUTHORS" "$DEST/"
cp -v "$GITHUB_WORKSPACE/LICENSE" "$DEST/"
cp -v "$GITHUB_WORKSPACE/README.md" "$DEST/"

mkdir -p "$DEST/themes"
cp -v "$GITHUB_WORKSPACE/shared/themes/"* "$DEST/themes/"

mkdir -p "$DEST/translations"
cp -v "$BUILD_DIR/src/"*.qm "$DEST/translations/" || true

mkdir -p "$DEST/plugins"
cp -v "$BUILD_DIR/plugins/"*.dll "$DEST/plugins/"

# Copy test binary and its Qt dependency into the deployed directory
# so tests run against the fully deployed application layout.
cp -v "$BUILD_DIR/copyq-tests.exe" "$DEST/"
cp -v "$QT_ROOT_DIR/bin/Qt6Test.dll" "$DEST/"

# Point tests at the itemtests plugin in the build tree.
# The test harness picks this up via COPYQ_PLUGINS.
cp -v "$BUILD_DIR/src/itemtests.dll" "$DEST/"

# Copy OpenSSL libraries.
for lib in libcrypto libssl; do
    # Find the DLL regardless of exact version suffix.
    dll=$(find "$OPENSSL_ROOT_DIR" -maxdepth 2 -name "${lib}*.dll" -print -quit 2>/dev/null || true)
    if [[ -n "$dll" ]]; then
        cp -v "$dll" "$DEST/"
    fi
done

# Run windeployqt to gather Qt runtime dependencies.
crypto_libs=("$DEST/qca-qt6.dll" "$DEST/qt6keychain.dll")
kf_libs=("$DEST/KF6ConfigCore.dll" "$DEST/KF6Notifications.dll")

"$QT_ROOT_DIR/bin/windeployqt" \
    --no-translations \
    "${kf_libs[@]}" \
    "${crypto_libs[@]}" \
    "$DEST/copyq.exe"

echo "Deploy complete: $DEST"

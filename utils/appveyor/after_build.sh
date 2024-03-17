#!/usr/bin/bash
set -exuo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

mkdir -p "$Destination"

cmake --install "$BUILD_PATH" --config Release --prefix "$Destination" --verbose

if [[ $WITH_NATIVE_NOTIFICATIONS == ON ]]; then
    cp -v "$INSTALL_PREFIX/bin/KF5"*.dll "$Destination"
    cp -v "$INSTALL_PREFIX/bin/snoretoast.exe" "$Destination"
    kf5_libraries=(
        "$Destination/KF5ConfigCore.dll"
        "$Destination/KF5Notifications.dll"
    )
else
    kf5_libraries=()
fi

cp -v "$Source/AUTHORS" "$Destination"
cp -v "$Source/LICENSE" "$Destination"
cp -v "$Source/README.md" "$Destination"

mkdir -p "$Destination/themes"
cp -v "$Source/shared/themes/"* "$Destination/themes"

mkdir -p "$Destination/translations"
cp -v "$BUILD_PATH/src/"*.qm "$Destination/translations"

mkdir -p "$Destination/plugins"
cp -v "$BuildPlugins/"*.dll "$Destination/plugins"

cp -v "$OPENSSL_PATH/$LIBCRYPTO" "$Destination"
cp -v "$OPENSSL_PATH/$LIBSSL" "$Destination"

"$QTDIR/bin/windeployqt" --help
"$QTDIR/bin/windeployqt" \
    --no-system-d3d-compiler \
    --no-angle \
    --no-opengl-sw \
    --no-quick \
    "${kf5_libraries[@]}" \
    "$Executable"

# Create and upload portable zip file.
7z a "$APP.zip" -r "$Destination"
appveyor PushArtifact "$APP.zip" -DeploymentName "CopyQ Portable"

# This works with minGW, not msvc.
# objdump -x "$Destination/KF5Notifications.dll" | grep -F "DLL Name"
# objdump -x "$Destination/copyq.exe" | grep -F "DLL Name"

# Note: Following removes system-installed dlls to verify required libs are included.
rm -vf /c/Windows/System32/libcrypto-*
rm -vf /c/Windows/System32/libssl-*
rm -vf /c/Windows/SysWOW64/libcrypto-*
rm -vf /c/Windows/SysWOW64/libssl-*
OldPath=$PATH
export PATH="$GPGPATH":$Destination
"$Executable" --help
"$Executable" --version
"$Executable" --info
export PATH=$OldPath

choco install -y InnoSetup
cmd " /c C:/ProgramData/chocolatey/bin/ISCC.exe /O$APPVEYOR_BUILD_FOLDER /DAppVersion=$APP_VERSION /DRoot=$Destination /DSource=$Source $Source/Shared/copyq.iss"

installer="$APPVEYOR_BUILD_FOLDER/copyq-$APP_VERSION-setup.exe"
appveyor PushArtifact "$installer" -DeploymentName "CopyQ Setup"

# Test installer
cmd " /c $installer /VERYSILENT /SUPPRESSMSGBOXES"
"C:/Program Files/CopyQ/copyq.exe" --version

# Test installer can close the app safely
(
    # Wait for CopyQ to start
    "C:/Program Files/CopyQ/copyq.exe" ""
    cmd " /c $installer /VERYSILENT /SUPPRESSMSGBOXES"
    echo "Installation finished"
) &
installer_pid=$!
export COPYQ_LOG_LEVEL=DEBUG
"C:/Program Files/CopyQ/copyq.exe"
wait "$installer_pid"

gpgconf --kill all

echo "All OK"

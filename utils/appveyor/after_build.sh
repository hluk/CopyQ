#!/usr/bin/bash
set -exo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

Source=$APPVEYOR_BUILD_FOLDER
Destination=$APPVEYOR_BUILD_FOLDER/$APP
Executable=$Destination/copyq.exe
BuildPlugins=$BUILD_PATH/plugins/$BUILD_SUB_DIR

mkdir -p "$Destination"

cmake --install "$BUILD_PATH" --config Release --prefix "$Destination" --verbose

cp -v "$INSTALL_PREFIX/bin/KF5"*.dll "$Destination"
cp -v "$INSTALL_PREFIX/bin/snoretoast.exe" "$Destination"

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
    "$Destination/KF5ConfigCore.dll" \
    "$Destination/KF5Notifications.dll" \
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
export PATH=$Destination

export QT_FORCE_STDERR_LOGGING=1
export COPYQ_TESTS_RERUN_FAILED=1
"$Executable" --help
"$Executable" --version
"$Executable" --info
"$Executable" tests

# Take a screenshot of the app.
"$Executable" &
"$Executable" showAt 0 0 9999 9999

"$Executable" add "Plain text item"
"$Executable" add "Unicode: "
"$Executable" 'write(mimeText, "Highlighted item", mimeColor, "#ff0")'
"$Executable" 'write(mimeText, "Item with notes", mimeItemNotes, "Notes...")'
"$Executable" 'write(mimeText, "Item with tags", plugins.itemtags.mimeTags, "important")'
"$Executable" write text/html "<p><b>Rich text</b> <i>item</i></p>"
"$Executable" write image/png - < "$Source/src/images/icon_128x128.png"

# FIXME: This does not show notifications.
#        Maybe a user interaction, like mouse move, is required.
"$Executable" popup "Popup title" "Popup message..."
"$Executable" notification \
    .title "Notification title" \
    .message "Notification message..." \
    .button OK cmd data \
    .button Close cmd data

"$Executable" screenshot > screenshot.png

"$Executable" exit
wait

export PATH=$OldPath

appveyor PushArtifact screenshot.png -DeploymentName "App Screenshot"

choco install -y InnoSetup
cmd " /c C:/ProgramData/chocolatey/bin/ISCC.exe /O$APPVEYOR_BUILD_FOLDER /DAppVersion=$APP_VERSION /DRoot=$Destination /DSource=$Source $Source/Shared/copyq.iss"

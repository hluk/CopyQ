#!/usr/bin/bash
set -exuo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

export PATH=$Destination:$PATH

export QT_FORCE_STDERR_LOGGING=1

"$Executable" &
copyq_pid=$!
"$Executable" showAt 0 0 9999 9999

"$Executable" add "Plain text item"
"$Executable" add "Unicode: "
"$Executable" 'write(mimeText, "Highlighted item", mimeColor, "#ff0")'
"$Executable" 'write(mimeText, "Item with notes", mimeItemNotes, "Notes...")'
"$Executable" 'write(mimeText, "Item with tags", plugins.itemtags.mimeTags, "important")'
"$Executable" write text/html "<p><b>Rich text</b> <i>item</i></p>"
"$Executable" write image/png - < "$Source/src/images/icon_128x128.png"

# FIXME: Native notifications do not show up.
#        Maybe a user interaction, like mouse move, is required.
"$Executable" config native_notifications "false"
"$Executable" popup "Popup title" "Popup message..."
"$Executable" notification \
    .title "Notification title" \
    .message "Notification message..." \
    .button OK cmd data \
    .button Close cmd data

"$Executable" sleep 1000

screenshot_count=0
mkdir -p copyq-screenshots

screenshot() {
    screenshot_count=$((screenshot_count + 1))
    file=$(printf "copyq-screenshots/%02d - %s.png" "$screenshot_count" "$1")
    "$Executable" screenshot > "$file"
}

screenshot "App"

"$Executable" keys "Ctrl+P" "focus:ConfigurationManager"
for n in $(seq 9); do
    screenshot "Configuration Tab $n"
    "$Executable" keys "DOWN" "focus:ConfigurationManager"
done
"$Executable" keys "ESCAPE" "focus:ClipboardBrowser"

"$Executable" keys "Shift+F1" "focus:AboutDialog"
screenshot "About Dialog"
"$Executable" keys "ESCAPE" "focus:ClipboardBrowser"

"$Executable" keys "Alt+T" "focus:Menu"
screenshot "Tab Menu"
"$Executable" keys "ESCAPE" "focus:ClipboardBrowser"

"$Executable" keys "Ctrl+N" "focus:ItemEditorWidget"
"$Executable" keys ":Testing 1 2 3" "focus:ItemEditorWidget"
screenshot "Editor"
"$Executable" keys "F2" "focus:ClipboardBrowser"

"$Executable" keys "Ctrl+N" "focus:ItemEditorWidget" \
    ":New Item" "F2" "focus:ClipboardBrowser"
"$Executable" keys "F2" "focus:ItemEditorWidget" \
    "END" "ENTER" ":with Notes" "F2" "focus:ClipboardBrowser"
"$Executable" keys "Shift+F2" "focus:ItemEditorWidget" \
    ":Some Notes" "F2" "focus:ClipboardBrowser"
screenshot "Items"

7z a "copyq-screenshots.zip" -r "copyq-screenshots"
appveyor PushArtifact "copyq-screenshots.zip" -DeploymentName "CopyQ Screenshots"

"$Executable" exit
wait "$copyq_pid"

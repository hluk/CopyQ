# CopyQ on GNOME Wayland: history stays empty, items get lost, "Item Content" windows pile up

Tested on Ubuntu (GNOME, Wayland) with the distro package CopyQ 7.1.0 / Qt 5.15.

## Symptoms

- CopyQ is running and monitoring is on, but `copyq count` stays at `0`.
- `~/.local/share/copyq/copyq/copyq.log` repeats:
  ```
  QtWarning: Activating Wayland clipboard took 5000 ms
  QtCritical: Failed to activate Wayland clipboard
  ```
- Items copied shortly before a logout or a GNOME Shell crash disappear.
- Many "Item Content" windows ("Teor dos itens" in pt_BR) open on top of each other.

## Causes

1. GNOME does not implement the `wlr-data-control` protocol. A native Wayland
   Qt app cannot read the clipboard while it is in the background, so CopyQ
   never sees new clipboard content. Running CopyQ through XWayland
   (`QT_QPA_PLATFORM=xcb`) fixes it, because Mutter syncs the Wayland
   clipboard to XWayland clients. CopyQ 10+ applies this automatically to the
   clipboard monitor process; 7.x does not.
2. Large `save_delay_ms_on_item_*` values (for example `300000` = 5 minutes)
   keep new items only in memory. If GNOME Shell dies, CopyQ dies with it
   before writing the tab file.
3. `F4` is bound to "Show Content" (`show_item_content`). Each press, or an
   auto-repeated key event, opens another "Item Content" dialog.

## Fix with the official CopyQ (no fork needed)

Option A: upgrade to CopyQ 10 or newer from https://github.com/hluk/CopyQ/releases
or the official PPA (`ppa:hluk/copyq`) and enable the bundled
"CopyQ Clipboard Monitor" GNOME Shell extension.

Option B: keep your current version and apply these settings.

1. Start CopyQ under XWayland. Edit `~/.config/autostart/copyq.desktop`:
   ```
   Exec=env QT_QPA_PLATFORM=xcb /usr/bin/copyq
   ```
   Do the same in a copy of the menu launcher:
   ```
   cp /usr/share/applications/com.github.hluk.copyq.desktop ~/.local/share/applications/
   # then set:
   Exec=env QT_QPA_PLATFORM=xcb /usr/bin/copyq --start-server show
   ```
2. Save items quickly. With CopyQ closed (`copyq exit`), set these in
   `~/.config/copyq/copyq.conf` under `[Options]`:
   ```
   save_delay_ms_on_item_added=1000
   save_delay_ms_on_item_modified=1000
   save_delay_ms_on_item_moved=1000
   save_delay_ms_on_item_removed=1000
   save_delay_ms_on_item_edited=1000
   ```
3. Stop the "Item Content" windows. Under `[Shortcuts]`:
   ```
   show_item_content=
   ```
   Or use Preferences > Shortcuts > "Show Content..." and clear the shortcut.
4. Restart:
   ```
   QT_QPA_PLATFORM=xcb setsid /usr/bin/copyq >/dev/null 2>&1 &
   ```

Or run the script in this folder, which applies all steps and keeps `.bak`
backups:

```
bash fix-copyq-gnome-wayland.sh
```

## Verify

```
copyq count            # copy something in another app, the number goes up
copyq read 0           # shows what you just copied
copyq exit && QT_QPA_PLATFORM=xcb setsid copyq &   # items are still there
```

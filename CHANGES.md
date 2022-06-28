# 6.2.0

## Added

- Tabs can now load at least some items from a partially corrupted data file
  dropping the rest of the items.

- Simpler and safer data saving uses Qt framework (`QSaveFile`).

- New `Settings` class in scripts can be used to manage INI configuration
  files (#1964).

## Changed

- Obscure untested Save button has been removed from Action dialog.

## Fixed

- Fixes restoring window geometry in a loop (#1946).

- Fixes converting internal byte array representation in scripts in some rare
  cases.

- Windows: Fixes exiting the app on logout (#1249).

- Windows: Workaround to treat native path separators properly and not as
  special escape characters.

# 6.1.0

## Added

- Users can now customize shortcuts for the built-in editor (#708).

- Users can now set default style sheet for HTML items to override for example
  color for hyperlinks with `a { color: lightblue }` (#1859). The new settings
  can be found under Item configuration tab under Text sub-tab.

## Changed

- Window geometry (size and position) restoring is now simpler: The app sets
  geometry only initially and when the current screen/monitor changes.

  The mouse cursor position indicates the current screen. In case the app
  cannot inspect the mouse pointer position (for example on some Wayland
  compositors), it is left up to the window manager to decide to move the
  window to another screen.

  Users can still disable the automatic geometry by running the following
  command (in Action dialog or terminal) and restarting the app:

      copyq config restore_geometry false

## Fixed

- Fixes moving items in synchronized tabs after activating them from the
  context menu (#1897).

- Windows: Fixes tray icon tooltip (#1864).

- Windows: External editor command now treats native path separators properly
  (#1894, #1868).

- macOS: Fixes crash when pasting from the main window or menu (#1847).

- macOS: Older versions of macOS (down to 10.15) are now supported again
  (#1866).

- Wayland: Fixes using correct window title icon (#1910).

- Wayland: Fixes retrieving UTF-8 encoded text from selection in environments
  which supports it.

- Wayland: Fixes restoring window size without breaking window position (window
  position cannot be set in most or all Wayland compositors).

# 6.0.1

## Fixed

- X11: Fixes global/system-wide shortcuts (#1860).

# 6.0.0

## Added

- Native notifications now have lower urgency if the display interval is less
  than 10 seconds. This makes clipboard change notification less intrusive.

- Preview dock can be focused with Tab key (#1826). Escape, Tab or Shift+Tab
  returns focus back to the item list.

- All options are now documented/described when using command `copyq config`.

- Command editor now supports highlighting multi-line strings enclosed by
  backticks (#1845).

- New option to disable restoring window/dialog geometry (app needs to be
  restarted after changing the option):

      copyq config restore_geometry false

- macOS: New option to enable native tray menu (#1652):

      copyq config native_tray_menu true

- Support for building the source code with Qt 6 framework.

## Changed

- While search bar is focused, pressing Down or PageDown key now selects next
  item without focusing the item list (#1834).

- Internal commands (like ""Show/hide main window", Pin/Unpin, Encrypt/Decrypt)
  will now be automatically updated in following application releases or
  whenever the language changes. The side-effect is that only icon, shortcuts,
  enabled state and list order can be changed for these commands. Old internal
  commands added in previous versions (5.0.0 and lower) of the app need to be
  removed manually.

- Increases the default delay for storing changed clipboard owner. This can
  help save correct window title for new clipboard content when the window is
  closed right after the copy operation. The delay can be changed using:

      copyq config change_clipboard_owner_delay_ms 500

- The application version now excludes the "v" prefix in UI and CLI.

- Log Qt warnings by default (at Warning log level messages).

- Linux: Other data formats are now stored for primary selection so as to
  support some automatic commands properly (for example, ignore selection when
  it contains a special format). Images and non-plain text formats are still
  ignored for performance reasons.

## Fixed

- Drag'n'drop operations are now properly ended (#1809).

- Main window will now open only inside the visible screen area (#1768).

- "Clear Current Tab" command will no longer show a message dialog if there are
  pinned items (#1823).

- Improves initial size for native tray menu.

- Fixes removing backup file for old commands configuration.

- Fixes broken item selection state (#1828).

- Fixes hiding main window immediately when shown. This can be caused by long
  animations in window manager.

- Further performance improvements for logging, application startup and file
  synchronization.

- Linux: Native status icon (using D-Bus) is used by default instead of the
  legacy tray icon. Application start delay/sleep hacks should no longer be
  needed (#1526).

- Wayland: Improved clipboard access.

- Wayland: Fixes selection/clipboard synchronization.

- Windows: Any application instance is now closed automatically before
  installation.

# v5.0.0

## Added

- Search matches similar accented characters (#1318). For example, searching
  for "vacsina" would also show items containing "väčšina".

- If the clipboard tab is renamed, clipboard will be still stored in the
  renamed tab. Similarly if a specific tab is set for tray menu. This basically
  modifies `clipboard_tab`, `tray_tab` options when renaming tabs.

- New predefined command to clear the current tab.

- Tabs can be reordered in Preferences (in addition to tab bar/tree).

- Tabs can be reordered from command line or a script. For example:

      copyq 'config("tabs", ["&clipboard", "work", "study"])'

- New buttons can move commands, tabs and plugins in configuration to top and
  bottom with a single click. This previously required dragging item to the
  top/bottom or multiple clicks on the move up/down buttons.

- Script function `dialog()` supports non-editable combo box. For example:

      var choice = dialog('.combo:Select', ['a', 'b', 'c'])

- Script function `dialog()` restores last position and size of dialog
  windows with matching title (set with `.title`).

- Syntax highlighting for more script keywords.

- New script class `ItemSelection` allows more powerful, consistent, safe and
  fast handling of multiple items. Examples:

      // move matching items to the top of the tab
      ItemSelection().select(/^prefix/).move(0)

      // remove all items from given tab but keep pinned items
      ItemSelection(tabName).selectRemovable().removeAll();

## Changed

- Simpler lock file mechanism is used instead of a system semaphore and shared
  memory lock (#1737). This allows to support more platforms.

- Editor font from Appearance settings is used for the edit widget in Command
  and Action dialogs (#1757).

- Theme does not modify the scrollbar in item list by default (#1751).

## Removed

- Windows: Migrating old configuration from registry to file format is no
  longer supported.

## Fixed

- Icons are rendered properly in About dialog. This uses correct icon font
  from the app instead the one installed on the system.

- Correct UI layout direction is used depending on the selected language
  (#1696).

- Automatic commands that use regular expressions for matching
  window title or clipboard content are imported properly
  (hluk/copyq-commands#45).

- Native notifications are updated correctly when using existing notification
  ID.

- Bash completion script is installed to a correct path.

- macOS: Fixes pasting/copying when using different keyboard layouts (#1733).

- macOS: Avoids focusing own window before paste operation (#1601).

- macOS: Tries to paste directly to the process ID if the window ID is not
  available (#1395) (#1686).

# v4.1.0

- Old notification system can now be used instead of native/system
  notifications (#1620). This can be disabled in Notifications tab in
  Preferences.

- Additional configuration file for notifications will not be created
  automatically (#1638).

- In scripting, `console` object can be used for logging, measuring elapsed
  time and asserting conditions.

- `plugins.itempinned.mimePinned` contains item data format for pinned items
  (item is pinned if it contains the format).

- Command completion menu contains more complete list of script
  objects/function and better description.

- Action dialog command, `action()` and commands (if "Content"/filter regular
  expression is unset) now do not replace `%2` through `%9`. This allows
  passing URLs without requiring to escape encoded characters like `%20` or
  `%3A`.

- Syntax highlighting for hexadecimal and boolean values in the command editor.

- Fix moving the main window to different display/screen (#1624).

- Windows: Native notifications are disabled on Windows 7 (#1623). This fixes
  crash because of unsupported features.

- Windows: Fixed crash when loading some themes (#1621).

- Wayland: Restores last stored geometry for a window (since getting current
  screen does not work).

- MinGW Windows builds are available again (without native notification
  support).

# v4.0.0

## Features

- Synchronization plugin newly keeps order of new items consistent between
  multiple application instances (#1558). Newly added items in one instance
  will appear at the top of other instances.

- Search now finds separate words if regular expressions are disabled (#1569).
  Searching for "foo bar" will find items containing both "foo" and "bar" and
  the relative position of words no longer matter.

- System notification popups are now used instead of own implementation.

- Item rows in main window and tray menu are now indexed from one instead of
  zero by default (#1085). This can be reverted to the old behavior using
  command `copyq config row_index_from_one false`.

- A tag can be marked as "locked" in configuration. Items with such tags cannot
  be removed until the tag is removed or "unlocked".

- Command line completion for bash (#1460). Thanks, Jordan!

- History combo box is focused when Action dialog opens to easily recall recent
  commands. Note: Focusing combo boxes is not supported on macOS.

- Web plugin has been completely dropped (unmaintained with performance and
  possible security problems). Simple HTTP rendering is still supported by Text
  plugin.

- Advanced option `window_paste_with_ctrl_v_regex` to change default paste
  shortcut Shift+Insert to Ctrl+V for specific windows (only on Windows and
  Linux/X11). This is regular expression matching window titles.

- New advanced options allow to set intervals and wait times for copying,
  pasting and window focus:

  * `script_paste_delay_ms` - delay after `paste()`, default is 250ms (#1577)
  * `window_wait_before_raise_ms`
  * `window_wait_raised_ms`
  * `window_wait_after_raised_ms`
  * `window_key_press_time_ms`
  * `window_wait_for_modifiers_released_ms`

- Format "text/plain;charset=utf-8" is now preferred to "text/plain".

- FakeVim: Auto-indents when adding new lines.

## Scripting

- New scripting engine. This adds some new functionality, better ECMAScript
  support, improved performance and would allow Qt 6 support in the future.

- Argument `--start-server` to both starts the app if not yet running and runs
  a command (#1590). For example, `copyq --start-server show` would show main
  window even if the app was not started yet.

- Accessing a missing plugin from script throws an human-readable error and
  show an popup if uncaught (for example, "plugins.itemtags" could throw
  "Plugin itemtags is not installed").

- Script function `setPointerPosition()` throws an error if it fails to set the
  mouse pointer position.

- Fixes for `NetworkReply` objects to properly fetch data when needed (#1603).
  Script functions `networkGet()` and `networkPost()` now wait for data to be
  fetched. New script functions `networkGetAsync()` and `networkPostAsync()`
  can be used to make asynchronous network request. Property
  `NetworkReply.finished` can be used to retrieved completion status of a
  request.

- New script function `styles()` to list possible application styles and option
  `style` to override the default or current style.

## Platforms

- Wayland support, notably clipboard access and window size restoring.

- Windows: Builds are now 64bit (built by Visual Studio tools).

- Linux: Selecting the app icon in the desktop environment using the installed
  entry in the application menu or launcher, shows main window immediately.
  Previously, the app started silently in tray or minimized state.

- Linux/X11: Fixes copying from VirtualBox (#1565).

- macOS: Fix version information (#1552).

## User Interface

- The default theme is kept consistent with system theme (#1613). This also
  allows to use new special placeholders like `default_bg` and `default_text`
  in custom style sheet files.

- Command dialog always shows the command type at top.

- Updated icons (Font Awesome 5.15.3).

- FakeVim: Command line not supports better text interaction (select, copy,
  cut, paste).

## Fixes

- Fix crashed with some custom system themes (#1521).

- Fix importing old saved tabs/configuration (#1501).

- Fix trailing spaces in copied commands.

- Fix filtering shortcuts in preferences.

- Fixes for window geometry restoring.

- Tray menu items are updated only just before the menu is shown.

- Avoid storing "text/richtext" by default since displaying of this format is
  not supported.

- Better performance when updating synchronized items.

- Various appearance and theme fixes (#1559).

## Various

- Code base now follows C++17 standard.

- GitHub Actions now continuously build and test for Linux and macOS, and
  provide development builds for macOS.

# v3.13.0

- Newly, if a global shortcut is triggered when the main window is active, the
  command will be executed with item selection and item data available (#1435).

- New `focusPrevious()` script function to activate window that was focused
  before the main window.

- Export now write data to a temporary file before saving.

- Display command are now also applied on item preview (e.g. to enable syntax
  highlighting in the preview).

- New command line option "tray_menu_open_on_left_click" to check default mouse
  button behavior for tray icon (`copyq config tray_menu_open_on_left_click
  true`).

- New command line option "activate_item_with_single_click" to activate items
  with single click (`copyq config activate_item_with_single_click true`).

- New command line options "filter_regular_expression" and
  "filter_case_insensitive" to change the item search behavior.

- New command line option "native_menu_bar" to disable native/global menu bar
  (`copyq config native_menu_bar false`).

- Updated icons (Font Awesome 5.15.1)

- Improved performance of loading the icon font.

- Fix crash when exporting large amount of data (#1462).

- Fix entering vi search mode (#1458).

- Fix size of scrollable text area in item preview (#1472).

- OSX: Broken native/global menu bar was replaced by default with application
  menu bar (#1444). This can be changed with `copyq config native_menu_bar true`.

- OSX: Mouse click on tray icon is now handled similarly to other platforms.
  This can be changed with `copyq config tray_menu_open_on_left_click true`.

# v3.12.0

- Unsaved data are now saved whenever application is unfocused, otherwise
  immediately after an item is edited and saved or after ~5 minute intervals if
  items change. These intervals can be configured - use `copyq config` to list
  options with `save_delay_` prefix. For example, disable storing after an item
  is added (only when app is unfocused, exits or tab is unloaded):

      copyq config save_delay_ms_on_item_added -1

- Filter field in commands can now modify menu items. This is done by setting
  properties to "menuItem" object. Example:

      copyq:
      menuItem['checkable'] = true
      if (plugins.itempinned.isPinned.apply(this, selectedItems())) {
          menuItem['checked'] = true
          menuItem['text'] = 'Unpin'
          menuItem['color'] = '#f00'
          menuItem['tag'] = 'X'
      } else {
          menuItem['checked'] = false
          menuItem['text'] = 'Pin'
          menuItem['icon'] = ''
      }

- Application icon will no longer automatically change when there is an ongoing
  operation (i.e. the icon snip animation). This caused performance issues in
  some environments and it was not tested automatically so it often broke. When
  clipboard storing is disabled the icon only changes opacity slightly.

- New `preview()` script function shows/hides item preview.

- Avoid terminating application on SIGHUP (#1383)

- Use brighter bar for pinned items with a dark theme (#1398)

- Improved notification text line wrapping (#1409)

- Improved layout when showing many shortcut buttons (#1425)

- Fix indentation when importing commands with CRLF (#1426)

- Fix using the configured notification font (#1393)

- Fix initial item size (avoid scroll bars)

- Fix decrypting item with note

- Fix hiding windows after changing "Always on Top" option

- Fix tool bar flickering when browsing items

- Fix crash when destroying main window

- Fix rare crash when menu items change

- FakeVim: Improved completion menu control with Vim emulation

- FakeVim: Always start in normal mode

- FakeVim: Fix searching backwards

- Windows: Paste operation is now postponed until user releases shortcut
  (#1412). This works better than releasing the shortcut keys automatically and
  is consistent with behavior on Linux.

- Windows: Fix SSL/TLS errors; `networkGet()` should now work with `https`
  protocol

- Windows: Fix native GUI style (#1427)

# v3.11.1

- Fix scrolling in selected text items (#1371)

- Fix using application icon font instead of a system-installed on (#1369)

- X11: Fix checking correct text before selection synchronization

# v3.11.0

- Tab character size can now be set (number of spaces) and maps more accurately
  to space character width (#1341). The default value is 8 spaces which is
  smaller than before. This can be changed on command line:

      copyq config text_tab_width 4

- New `move()` script function moves selected items within tab.

- New `menuItems()` script function creates custom menus.

- CSS stylesheet files to fully customize appearance

- Allow keyboard navigation in item preview dock

- "Show Preview" is now available in File menu instead of Item menu

- Shortcuts Ctrl+P and Ctrl+N selects previous/next action in Action dialog.

- New synchronized item/files are now added to item list ordered alphabetically
  by filename which is faster and more consistent on multiple platforms than
  ordering by modification-time (#833).

- Improved performance when synchronizing items/files

- Non-owned synchronized files at the end of item list are now dropped (but not
  deleted) if the list is full.

- Updated icons (Font Awesome 5.13.0)

- Simpler item scrollbar style

- Omit showing new notification under mouse pointer (#1310)

- Duplicate "CopyQ Error" notification are not shown

- Bind x to Delete in Vi style navigation mode

- Left/Right arrow keys now work in FakeVim editor mode by default

- FakeVim editor mode now supports cutting text to given register

- If FakeVim editor mode is active in a dialog, Esc key allows to close the

- Consistent window and dialog titles in application (" - CopyQ" suffix)
  dialog.

- Fix opening tray menu with empty search

- Fix search in main window and tray with different keyboard layouts (#1316)

- Fix restoring search when closing internal editor

- Fix crash when synchronizing pinned items/files (#1311)

- Fix pasting synchronized file instead of its content (#1309)

- Fix enabling menu items with filters in commands (#1284)

- Fix commands for removing tags from items (#1332)

- Fix copying from item preview dock (#1326)

- Fix position of main window on current screen (#1216)

- Fix copying "text/plain;charset=utf-8" format as a text (#1324)

- Fix preview search highlight in Appearance configuration (#1354)

- Fix hover/mouse-over style for items (#1347)

- Fix wrapping long notification text

- Fix jitter when scrolling in item list

- Fix item width

- X11: Fix re-getting clipboard content after aborted (#1356)

- Windows: Use builds with Qt 5.13

# v3.10.0

- Use environment variable `COPYQ_DEFAULT_ICON=1` to show the original
  application icon instead of the one from current icon theme.

- Avoid updating menu too unnecessarily

- Drop using deprecated Qt API and require at least Qt 5.5 (meaning Ubuntu
  14.04 and Debian 8 are no longer supported)

- Use non-native color and font dialogs which fixes showing these in Gnome/Gtk

- Fix GUI with fractional scaling

- Fix updating tray menu (remove empty sections)

- Fix editing synchronized file content instead of its path

- OSX: Omit preventing system from entering the sleep mode

- X11: Improve clipboard/selection synchronization

- X11: Avoid reading clipboard in parallel in the monitor process

# v3.9.3

- New `loadTheme()` script function loads theme from INI file.

- Currently selected item stays on top on PageUp/Down (less jittery list view
  scrolling)

- Performance improvements: Updates GUI only when necessary; dedicated
  processes to run menu filters and display commands; reloads configuration
  once when setting multiple options with `config()`

- Skips using a command from a disabled plugin

- Logs information on slow menu filters and display commands

- Fix hiding item preview when disabled (caused an empty window to be shown)
  and when no items are selected

- Fix taking screenshots on multiple monitors

- Fix duplicate show/hide tray menu items

- Fix moving synchronized items to top when activated

- Fix calling `onExit()` second time when on shutdown

- Fix removing empty actions from history in Action dialog

- Fix updating version from Git when rebuilding

- OSX: Fix refocusing correct main window widget

- Windows: Use Qt 5.12.5 builds (MinGW 7.3.0 32-bit, msvc2017 64-bit)

# v3.9.2

- Fix unnecessary tab reloading after expired.

- Fix repeated menu updates.

- Fix loading tabs with an empty item

- Fix initializing expire timeout (it was always 0 or 1 minute)

# v3.9.1

- Commands are moved to a separate configuration file "copyq-commands.ini".

- Horizontal tabs in the configuration dialog were replaced with a list of
  sections so it's possible to view all of the sections even in a smaller
  window.

- New option `hide_main_window_in_task_bar` to hide window in task bar can be
  set using `copyq config hide_main_window_in_task_bar true`.

- New `logs()` script function prints application logs.

- New `clipboardFormatsToSave()` script function allows to override clipboard
  formats to save.

- Some hidden options can be modified using `config()` script function.

- Font sizes in items and editor are limited to prevent application freeze.

- Application icons are cached so as to avoid creating icons for the snip
  animation again.

- Fix restoring tabs with some non-ASCII characters

- Fix opening window on different screen with different DPI

- Fix 100% CPU utilization on wide screens (5120x1440)

- Fix icon size and GUI margins in Tabs configuration tab

- X11: Fix stuck clipboard access

- X11: Faster selection synchronization

- OSX: Prevent showing font download dialog

- OSX: Fix clipboard owner window title

# v3.9.0

- Large images in clipboard are no longer automatically converted to other
  formats - it caused slowdowns and was mostly unnecessary since some usable
  image format was provided.

- The server/GUI process now provides the clipboard and no other process is
  started (i.e. "copyq provideClipboard"). The other process helped to unblock
  GUI in rare cases when an application requested large amount of clipboard
  data, but it could cause some slowdowns to start the process.

- Closing external editor focuses the edited item again (if the editor was open
  from main window).

- Only Global Shortcut commands are shown in tray menu.

- Separate Global and Application shortcuts into tabs in configuration dialog.

- New per-tab configuration allows disabling storing items on disk and limiting
  number of items.

- New script function unload() and forceUnload() allow unloading tabs from main
  memory.

- Tabs synchronized with a directory on disk are updated only when needed. An
  update happens regularly when synced tab has focus or item data and are too
  old. Update interval is 10s and can be changed by setting env variable
  COPYQ_SYNC_UPDATE_INTERVAL_MS (in ms).

- X11: New option allows to disable running automatic commands on X11 selection
  change.

- Fix rare crash on exit.

- Fix value returned by filter() after it's hidden

- Fix memory leak (tool bar and menu items were not properly cleaned up)

- Fix crash when opening content dialog with many formats

- OSX: Fix pixelated UI rendering on retina displays.

- Windows: Fix blocking Ctrl, Shift and other modifier keys after invoking
  paste action. The downside is that it's no longer possible to invoke command
  assigned to a global shortcut repeatedly without releasing these keys.

- X11: Fix high CPU usage when mouse selection cannot be accessed.

- X11: Fix assigning global shortcuts with keypad keys

# v3.8.0

- Custom settings from scripts (using settings() function) are now saved in
  "copyq-scripts.ini" file in configuration directory. Existing configuration
  needs to be moved manually from "[script]" section in the main configuration
  file ("copyq info config" command prints the path) to "[General]" section to
  the new file (in the same directory).

- Correct clipboard owner (window title) is now used when the window is hidden
  after copy operation (e.g. password manager copies password and hides its
  window immediately).

- New script functions onStart and onExit allow to defined commands run when
  the application starts and exits.

- New script functions pointerPosition and setPointerPosition to get/set mouse
  cursor position on screen.

- New script callback onClipboardUnchanged called when clipboard changes but
  monitored content remains the same.

- Block default shortcut overridden by a command while its filter command needs
  to run.

- Item selection is not cleared when main window hides in response to
  activating an item or automatically when unfocused.

- Clipboard dialog opens much faster and retrieves clipboard data only when
  needed.

- Clipboard dialog contains special clipboard formats and the whole list is
  sorted - plain text first, HTML, other text, application, application
  specific (`application/x-`), special (uppercase).

- Detect encoding for other text formats.

- Method text for ByteArray returns correctly formatted text from unicode
  encoded data (e.g. UTF-8).

- Show pin and tag menu items even if shortcut is not assigned (can be disabled
  completely in Command dialog).

- Hide encrypt/decrypt commands when keys for Encrypt plugin don't exist.

- Command list is focused when Command dialog opens; the less important "Find"
  field is smaller and moved below the list.

- Process manager is completely redone and the dialog is no longer created at
  application start (faster application start, smaller memory footprint).

- Process manager has filter field for searching for commands.

- Process manager has new column showing error message.

- Process manager has color status icons for running, starting and failed
  processes.

- Next/Previous formats are no longer available (were rarely used and
  untested).

- Updated donation link: https://liberapay.com/CopyQ/

- FakeVim, if enabled, is used for other multi-line text fields in the
  application (e.g. item notes, command editor).
- FakeVim, if in a dialog, binds save and quit command to the dialog buttons -
  `:w` for Apply, `:wq` for OK, `:q` for Cancel.
- FakeVim status bar shows an icon for errors and warnings.
- FakeVim now handles set commands correctly.
- FakeVim text cursor is gray if the editor is not focused.

- Fix opening image editor for encrypted items.
- Fix opening SVG image editor if the bitmap one is unset.
- Fix stopping client processes properly.
- Fix showing main window under mouse pointer (with showAt function).
- Fix client crash when calling a method without instance (e.g. command "copyq
  ByteArray().size").

- OSX: Fix opening main window above full screen window.
- OSX: Fix selecting item with Up/Down keys when searching.

- X11: Fix setting wrong window title for own clipboard.
- X11: Fix synchronizing selection if the change is quick.
- X11: Fix tray icon on KDE.

# v3.7.3
- Search and item selection reset when main window is closed
- Updated icons (Font Awesome 5.6.3)
- Tray icon animation is not triggered if no automatic commands are run.
- Improved color themes on some systems
- Omit auto-hiding main window when it has a dialog open
- Fix transparency of some icons
- Fix size of menu when open on different screen
- Fix window geometry restore and rendering issues
- Fix auto-hide main window (e.g. on Gnome when using Activities)
- X11: Fix small tray icon on Gnome
- X11: Fix icon mask file name according to standard ("copyq_mask")

# v3.7.2
- Backspace deletes last character in tray menu search instead of clearing it
  completely
- Window title shows "<ITEMS>" label when non-text items are copied
- Command dialog uses simpler layout for easier command editing
- Command dialog shows simple command description
- Remove empty lines at the end of copied and exported commands
- New script functions to calculate hash: md5sum, sha1sum, sha256sum, sha512sum
- Retrieving clipboard data is interrupted if it's slow
- Editing commands no longer causes high CPU usage
- Completion menu for command editor is resized once
- Items are rendered faster and are shown with incorrect size initially while
  scrolling instead of showing empty items
- When Action dialog opens, the command editor is focused instead of the combo
  box containing command history (this consistent with default focus behavior
  on OS X)
- Clipboard monitor process loads configuration only at start
- Autostart option now works in the Flatpak package
- OSX: Application no longer crashes when using the main window close button
- X11: TIMESTAMP clipboard format is used to avoid retrieving unchanged content
  unnecessarily
- X11: Data installation path can be overridden with CMake options
  (CMAKE_INSTALL_DATAROOTDIR and DATA_INSTALL_PREFIX)
- X11: Store current window title with new clipboard data right after
  clipboard-change signal is received

# v3.7.1
- Store formats specified in Format field in automatic commands
- Fix restoring geometry on screens with different scaling factors
- X11: Fix restoring geometry on i3 window manager
- X11: Fix the first clipboard/selection signal when unchanged

# v3.7.0
- New option to show notes beside item content
- Removed option to show icon instead of notes
- Only plain text is Copied/Pasted from menu if Shift key is pressed
- Customizable shortcut for Item context menu
- Remove unmaintained Data plugin (can be replaced with a script)
- Allow to set icon to tab groups in tree view
- Allow export even if a tab group or an unloaded tab is selected
- Automatic commands are no longer run in clipboard monitor context
- Omit aborting monitor by calling abort() from automatic commands
- Omit aborting automatic commands by changing configuration
- Updated icons (Font Awesome 5.4.2)
- Fix sizes of items with notes and when using Web plugin
- Fix icons alignment
- Fix setting different font weights in Appearance configuration
- Fix button sizes in Appearance configuration
- Fix position of the context menu for large items
- Fix server crash when a client disconnected while processing its request
- Fix crash when changing icon or renaming unloaded tab
- Fix handling of incorrect editor command
- X11: Faster and safer clipboard checking and synchronization
- X11: Prioritize checking clipboard before selection

# v3.6.1
- Omit displaying notes twice when "Show simple items" is enabled
- Fix broken tab decryption

# v3.6.0
- Invoking search with a shortcut reuses last search expression
- Exiting from search (ESC) doesn't unselect found item
- `COPYQ_SETTINGS_PATH` environment variable overrides default config path
- Merge top item with same new clipboard text
- Check clipboard after start
- Animate app icon when a clipboard changes or a client calls some functions
- Use gpg for encryption if gpg2 is unavailable
- Faster tray and context menu updates
- Close dialog() after client process exits
- Display system, arch and compiler info when using version()
- Tests are about 2x faster
- Updated icons (Font Awesome 5.3.1)
- Fix search field icon position
- Fix overriding `onClipboardChanged()` and similar script functions
- Fix closing client processes
- Fix deleting temporary timer object
- Fix handling return values and abort() in afterMilliseconds()
- Fix icon font sizes and omit using system-installed font
- Fix showing <EMPTY> label
- X11: Fix showing window after using close button on Qt 5.11
- X11: Fix crash when UI scaling is too large
- Windows: Fix removing old DLLs with installer

# v3.5.0
- Icon for global shortcuts in Shortcut configuration tab
- Simpler icons (smaller installation footprint)
- Faster copying and pasting from the application
- Faster and simpler invocation for commands run automatically
- More compact Process Manager dialog
- Scriptable function select() waits for clipboard to be set
- Image masks for colorizing icons ("icon-mask" and "icon-busy-mask")
- Improved logging
- Updated icons (Font Awesome 5.0.13)
- Fix showing icons when "System icons" is enabled (Windows and OS X)
- Fix initial setup for encryption
- Fix storing SVG images and other XML formats with text
- Fix stopping clipboard monitor and other processes at exit
- Fix restarting monitor whenever script commands change
- Fix updating status in Process Manager
- Fix using tab() multiple times from script
- Fix building for Qt 5.11
- Windows: Use Qt 5.6 LTS version for released binaries
- OSX: Fix URI list and UTF-16 text clipboard formats
- X11: Faster clipboard/selection synchronization

# v3.4.0
- Fix icon sizes in menu
- Fix showing dialog() above main window
- Fix closing clipboard monitor and provider on exit
- Safer data serialization and communication protocol
- Smoother colorized application icon
- Faster pasting to target window
- Run script commands in own context
- Omit showing same notification multiple times
- Omit handling text/uri-list by default
- OSX: Fix opening main window from tray menu
- OSX: Fix exporting configuration
- OSX: Fix focusing own windows for pasting
- Linux: Fix crashing on Wayland
- X11: Fix showing incorrect clipboard content

# v3.3.1
- Mark tray menu item in clipboard
- Scroll view when dragging items to top or bottom
- Always use current tab name in new tab dialog
- Update clipboard label in tray menu immediately
- Raise last window after menu is closed
- Paste commands correctly even if pasted into text edit widget
- Unload unneeded tabs after exported/imported
- Omit slow data compression on export
- Fix queryKeyboardModifiers() script function
- Fix settings autostart option from script
- Fix warnings when trying to load bitmap icons as SVG
- OSX: Fix settings global shortcuts with some keyboard layouts
- OSX: Fix tray menu icon size
- X11: Fix Autostart option
- X11: Fix crash when icon is too big
- X11: Omit resetting empty clipboard and selection
- X11: Omit overriding new clipboard with older selection content

# v3.3.0
- Add option for searching numbers in item list and tray menu
- Use exception instead of return code for exportData()/importData()
- Draw icon shadow (for internal icon font)
- Remove support for Qt 4, require Qt >= 5.1.0
- Fix storing only non-empty clipboard items
- Fix opening web browser from script with open()
- Fix exiting clipboard provider process when not needed
- Fix exportData()/importData() with relative file paths
- Fix SVG app icon resolution in some panels
- Fix closing window after a menu is closed and window is unfocused
- Fix icons for command error notifications
- Fix warnings when using system icons
- Linux: Fixes for AppData, desktop and flatpak files
- OSX: Fix pasting items
- OSX: Log errors when global shortcut registration fails

# v3.2.0
- Add option to close main window when unfocused
- Add script command type for enhancing scripting API and CLI
- Add display command type for overriding item display data
- Add documentation for plugins scripting API
- Add script function afterMilliseconds()
- Add isGlobalShortcut property to commands
- Allow to set global and menu command shortcuts in preferences
- New icon appearance (Font Awesome 5)
- Search in icon dialog (just start typing text)
- Improve scripting API for plugins
- Show command type with icon in command dialog
- Allow to set tray and window icon tag
- Allow to store MIME types with spaces
- Allow to set negative offsets for notifications
- Allow to override clipboard handling with script commands
- Script functions add() and insert() can add multiple items
- Hide vertical scroll bar in text items if not needed
- Hide main tool bar when internal editor is visible
- Run scripts safely in client process
- Omit closing internal editor if item changes
- Smoother scrolling and item browsing
- Fix accepting dialog() on Ctrl+Enter and Enter
- Fix sleep() timing out before interval
- Fix Dir().separator() return value type
- Fix item rendering
- Fix window title and tool tip for multi-line clipboard
- Fix tool bar rendering while editing an item
- Fix scaling pixel font sizes in HTML items
- Fix rendering item number in top left corner
- Fix rendering drag'n'drop preview on high-DPI screens
- Fix rendering notification icon on high-DPI screens
- Fix disabling antialiasing
- Fix opening menu/window on left screen (negative coordinates)
- Linux: Fix merging X11 selection if the first item is pinned
- Linux: Fix displaying tray menu on KDE/Plasma
- Windows: Fix negative item size warnings

# v3.1.2
- Don't show mouse cursor for selecting text after clicking on item
- Fix rendering background for item preview dock
- Fix showing main window under mouse pointer
- Fix loading tray icon
- Fix scrollbar interaction in items
- Fix performance for eliding huge text
- Fix correct mouse pointer in text items
- itemtext: Render plain text instead of empty HTML
- itemtext: Always limit text size in items
- itemweb: Use some sane settings for items
- itemencrypted: Copy to selection with copyEncryptedItems()

# v3.1.1
- Improve performance for items with long lines
- Linux: Fix tray icon

# v3.1.0
- Add "Paste current date and time" predefined command
- Add "Take screenshot" predefined command
- Add scriptable function queryKeyboardModifiers()
- Add scriptable function screenNames()
- Add scriptable function isClipboard()
- Add scriptable function toggleConfig()
- Add scriptable function iconColor()
- Allow to change icon color using COPYQ_SESSION_COLOR
- Expand text ellipsis if selected
- Avoid copying ellipsis if selected and copy rich text only if needed
- Improved command widget layout
- Copy encrypted items as hidden in UI
- Open external editor if internal fails
- Fix item rendering on high DPI screens
- Fix tray icon on high DPI screens
- Fix taking screenshots on multiple monitors and on high DPI screens
- Fix flicker when rendering items for the first time
- Fix icon layout for notes
- Fix showing icon if notes are empty
- Fix copying/drag'n'dropping files into a synchronized tab
- Fix long message alignment in notifications
- Fix activating simple items with double-click
- Fix styling of current and selected items
- Fix setting clipboard immediately after start
- Fix size of items with tags
- Fix moving multiple items to clipboard and to the top of the list
- Fix updating global shortcuts with setCommands()
- Fix clearing search after opening internal editor
- Fix aborting script execution
- Fix black scroll bar in items
- Fix completion popup resizing
- OSX: Fix some memory leaks
- Linux: Add manual pages
- Linux: Fix getting icon for non-default session from theme
- Linux: Fix settings tab name in KDE/Plasma
- Linux: Fix restoring with session manager
- Windows: Add pinned items to installer
- Windows: Fix saving tab with another plugin
- Windows: Fix pasting to a window
- Windows: Fix setting foreground window even if app is in background

# v3.0.3
- Added new documentation
- Added option to disable auto-completion for commands
- Improved image thumbnail rendering
- Fixed opening window on current screen
- Fixed item rendering when searching
- Fixed tab reloading and closing external editor
- Fixed image sizes
- Fixed loading plugins on OS X
- Fixed selecting area in screenshot
- Fixed rendering and showing tooltip for notes
- Fixed hang on exit when using QtCurve theme

# v3.0.2
- Added script functions for listing synchronized tabs and their paths
- Fixed showing window on current screen
- Fixed notification position with multiple screens
- Fixed rendering items when scrolling
- Fixed pasting from main window after switching tabs
- Fixed copy/paste to some apps on OS X
- Fixed focusing editor when closing completion popup on OS X
- Fixed setting temporary file template from script

# v3.0.1
- Install themes on OS X
- Improve pasting to current window
- Fix crash when the first tab is not loaded
- Fix crash when reloading tab after closing editor
- Fix item rendering and UI elements for high DPI displays
- Fix window focus after closing menu or main window on OS X
- Fix opening main window on current space on OS X
- Fix pasting to some windows on OS X
- Fix navigating item list
- Fix getting boolean from checkbox in dialog()
- Fix default move action for drag'n'drop
- Fix exitting on logout when tray is disabled

# v3.0.0
- Pinned and protected items
- Export/import tabs, configuration and commands in one file
- Create and modify commands from script
- Create temporary files from script
- Create notifications with buttons from script
- Take screenshots using script
- Allow to process lines on stdout from execute() scriptable using a function
- Safer and faster encrypt/decrypt commands (need to be re-added)
- Improved menu scriptable function
- Improved icon sharpness
- Improved plugin architecture
- Improved logging and displaying log
- Performance and memory consumption improvements
- Implemented copy() on OS X
- Fixed focusing menu and windows on OS X
- Fixed configuration folder path for portable version on Windows
- Fixed opening menu for a tab
- Fixed using correct GPG version for encryption
- Fixed tray menu position in KDE/Plasma

# v2.9.0
- Set text style in editor
- Search in editor
- Quick help in completion popup menu for commands
- Easier text selection in item preview
- Show whole text and unscaled image in item preview
- Improved pasting to windows on Linux/X11
- Fixed global shortcuts at application start on Linux/X11
- Fixed closing application from installer on Windows
- Fixed showing item preview at start
- Fixed saving position of new tabs and tab lists

# v2.8.3
- Search items from tray menu
- Added support for animated gifs (played when selected)
- Added special formats for automatic commands to sync and store clipboard
- Added auto-completion for command editor
- Added scriptable variables for MIME types
- Fix encryption with new OpenPGP
- Fix passing big data to commands on Windows

# v2.8.2
- Simplify appearance of items with notes and tags
- Support for drag'n'dropping images to more applications
- Added list widget for custom dialog
- Fixed opening windows on current screen
- Fixed tray icon appearance on Linux
- Fixed focusing tray menu from command
- Fixed dialog button translation on Windows
- Fixed passing big data to commands

# v2.8.1
- All Qt messages are logged
- Fixed and improved commands for Tags plugin
- Fixed removing last items when changing item limit
- Fixed library paths for OS X
- Fixed pasting items on Windows
- Fixed copying from script on Windows

# v2.8.0
- Insert images in editor
- Show simple items options
- Item preview window
- Move to Qt 5 on Windows and newer Linux distros
- Faster item content listing
- Simple filter for Log dialog
- Smooth icons on OS X
- Fixed system icons
- Fixed pasting animated images
- Fixed occasional crashes when finalizing commands with Qt 5
- Fixed opening log speed on Windows
- Lithuanian translation

# v2.7.1
- Colorize items with command
- Drag'n'drop items in selection order
- Fixed item selection with "next" and "previous" commands
- Fixed encrypting/decrypting items on Windows
- Fixed occasional client crashes at exit
- Fixed editor command on OS X

# v2.7.0
- Log accessible from GUI
- Performance and memory usage improvements
- Added scriptable function to set current tab (setCurrentTab())
- Added scriptable function to modify new items (setData())
- Appearance fixes
- Simplified window management
- Improved pasting to current window on Windows
- Window geometry fixes
- Command with Enter shortcut overrides item activate action

# v2.6.1
- Moved configuration from registry on Windows
- Fixed shortcuts on Windows
- Fixed window geometry restoring

# v2.6.0
- Show item notes in tray and window title
- Removed broken console executable on Windows
- Dutch translation
- Added env() and setEnv() to access and modify environment variables
- Access shortcut which activated command
- Fixed closing the application at shutdown on Windows
- Fixed some global shortcuts on Windows
- Fixed capturing some shortcuts

# v2.5.0
- Smarter tab name matching (ignore key hints '&')
- Fixed omit passing global shortcuts to widgets
- Fixed autostart option on Ubuntu
- Fixed window geometry saving and restoring
- Fixed reading binary input on Windows
- Fixed clearing configuration

# v2.4.9
- Added new light theme
- Added scriptable function focused() to test main window focus
- Customizable shortcuts for tab navigation
- Extended item selection
- Fixed tab expiration and updating context menu
- Fixed passing text to command from action dialog

# v2.4.8
- New command to show main window under mouse cursor or at a position with custom size
- Hide clipboard content when "application/x-copyq-hidden" is "1"
- "Copy next/previous item" command waits for clipboard to be set
- Fixed updating window title and tray tool tip on X11
- Fixed modifying multiple commands in Command dialog
- Fixed implicit date to string conversions

# v2.4.7
- Separate dialog for command help
- Added scriptable function visible() to check main window visibility
- Linux: Install bitmap icons for menus
- Linux: Install AppData file
- Allow to search for specific MIME types stored in items
- Menu items and customizable shortcut for cycling item format
- Fixed icon alignment
- Fixed moving tabs with Qt 5
- Fixed overriding socket file path (Linux and OS X)
- Fixed "Paste as Plain Text" command (Windows and OS X)
- Fixed tab tree layout and changing icons for tab groups
- Fixed URL encoding

# v2.4.6
- Fixed crash when removing command
- Fixed encryption/decryption selected items
- Fixed reading from standard input
- GUI fixes for high-DPI displays

# v2.4.5
- Option to save/restore history for filtering items
- Clipboard changes with unchanged content is ignored
- Notify about unsaved changes in command dialog
- Use application icons from current icon theme on Linux
- Simple error checking for user scripts
- Fix blocked system shutdown on Linux/X11

# v2.4.4
- Option to choose tab for storing clipboard
- Fixed overriding mouse selection (Linux/X11)
- Fixed window title updates from user commands
- Fixed toggling window visibility with Qt 5
- Minor GUI improvements and user command fixes

# v2.4.3
- Plugin for tagging items
- Plugins can provide script functions and commands
- Improved automatic commands execution
- Fixed gradients, transparency and other style improvements
- Fixed decryption with newer version of GnuPG
- Fixes for Qt 5 version

# v2.4.2
- Send input data to execute()
- Better clipboard encoding guessing
- Set tab icon from commands using tabicon()
- Fixed window title encoding on Windows
- Fixed restoring window geometry
- Performance fixes
- Various bug and usability fixes
- New logo

# v2.4.1
- Added scriptable classes File and Dir
- Added scriptable function settings() for saving custom user data
- Improved dialog() command
- Windows: Qt translated strings bundled with application
- Fixed %1 in command
- Fixed building with tests and Qt5

# v2.4.0
- Separate dialog for user commands and global shortcuts
- Search for item by row number
- Command highlighting
- More shortcuts can be mapped on Windows and X11
- New "copy" command to copy from current window to clipboard
- New "dialog" command to show dialog with custom input fields
- Fixed crash on log out on Windows
- Fixed clipboard monitoring on Windows
- Fixed argument encoding from client on Windows
- Fixed log output when printing messages from multiple processes
- GUI fixes

# v2.3.0
- Support for OS X
- Japanese translation
- Custom icons for tabs
- Show item count next to each tab name (optional)
- Added Process Manager for running and finished commands
- Scripting improvements
- Nicer format for copied user commands
- GUI fixes

# v2.2.0
- Custom system shortcuts for any user command
- Drag'n'drop items to tabs
- Options to set position and maximum size for notifications
- Option to open windows on same desktop
- Weblate service for translations (https://hosted.weblate.org/projects/copyq/master/)
- Commands input and output is UTF-8 only (this fixes encoding issues on Windows)
- Scripting engine improvements
- Various GUI improvements and fixes
- Fix main window position in various X11 window managers
- Fix crashing with Oxygen GUI style
- Fix storing images from clipboard on Windows
- Various GUI improvements and fixes

# v2.1.0
- French translation
- Save/load and copy/paste user commands
- Easier way to write longer commands and scripts
- Remove formats in clipboard and item content dialogs
- Command "toggle" focuses main window if unfocused (instead of closing)
- Choose log file and amount of information to log
- Lot of bugfixes and GUI improvements

# v2.0.1
- Initial OS X support
- Configuration moved into installed directory in Windows
- Change language in configuration
- New tool bar with item actions
- Option to apply color theme in tabs, tool bar and menus
- Allow to match items using a command
- Focus output item of the last executed command
- Allow to cancel exit if there are active commands
- Removed option to hide menu bar (inconsistent behavior)
- Fix showing lock icon in encrypted items

# v2.0.0
- Synchronize items with files on disk
- Faster tab loading and saving (data format was changed; only backward compatible)
- User can limit size of text items
- Opening external image editor fixed on Windows
- New logo and website
- Lot of other fixes

# v1.9.3
- Item and tab encryption (using GnuPG)
- FakeVim plugin for editing items (Vim editor emulation)
- Drag'n'drop items from and to list
- Improved appearance for notes
- Improved search bar
- New GUI for application and system-wide shortcuts
- Option to unload tabs after an interval
- Fixed item sizes and disabling font anti-aliasing
- Major bug fixes (mainly for Windows) and performance improvements

# v1.9.2
- Better performance
- GUI improvements and bugfixes

# v1.9.1
- Notifications -- customizable theme, timeout and position on screen
- Optional notification for new clipboard content
- Autostart option on Linux
- Reset empty clipboard to previous content
- More user-friendly item editor
- Optional font antialiasing
- Changed layout of configuration dialog
- Other fixes

# v1.9.0
- User notes
- Improved appearance settings with some example themes
- Tree view for tabs with groups
- Sessions, i.e. run multiple independent instances
- Lot of GUI improvements
- Compatibility with Qt5
- Bugfixes (crashing on Unity, icon colors etc.)

# v1.8.3
- Options to hide tab bar and main menu
- Automatic paste works with more applications under Linux/X11
- Multi-monitor support
- Lot of GUI fixes and improvements

# v1.8.2
- Added shortcut to paste current and copy next/previous item
- Bugfixes (paste to correct window, show tray menu on Unity, GUI and usability fixes)

# v1.8.1
- Spanish translation
- Option and system-wide shortcuts to temporarily disable clipboard storing
- Option for main window transparency
- Custom action on item activation
- Various GUI improvements and bugfixes

# v1.8.0
- New shortcuts: "Next/previous item to clipboard", "Paste as plain text"
- Show clipboard content in main window title and tray tooltip
- New options for commands (transform current item, close main window)
- GUI enhancements, faster application start with many tabs and items, lot of bugfixes

# v1.7.5
- User-settable editor for images
- Command-line fixes for Windows
- Commands for items of specified format (MIME type)
- Tray menu fixes

# v1.7.4
- Improved automatic paste from tray

# v1.7.3
- Paste immediately after choosing tray item
- German translation
- Support for system-wide shortcuts on Qt 5

# v1.7.2
- Clipboard content visible in tray tooltip

# v1.7.1
- Bugfixes for text encoding

# v1.7.0
- Plugins for saving and displaying clipboard content
- Bugfixes (lot of refactoring and tests happened)

# v1.6.3
- Some important bugfixes

# v1.6.2
- Dialog for viewing item content
- Improved tray menu
- Minor GUI updates

# v1.6.1
- Configurable tray menu
- Lot of fixes in GUI and bugfixes

# v1.6.0
- Highlight text and copy text in items
- Interactive web view
- Commands for any MIME type
- e.g. it's possible to create QR Code image from an URL and save it in list
- Pipe commands using '|' character

# v1.5.0
- Option to use WebKit to render HTML
- Wrap text with long lines
- Faster list rendering
- Icons from FontAwesome
- Desktop icon on Linux

# v1.4.1
- Support for other languages -- right now supports only English and Czech (any help is welcome)
- New "insert" command
- More safe item saving

# v1.4.0
- lot of GUI Improvements, faster interaction
- Automatic commands for matched windows (only on Linux and Windows)

# v1.3.3
- GUI Improvements
- New system-wide shortcuts
- Item editing improved

# v1.3.2
- Drag'n'Drop to clipboard
- "Always on Top" option
- Change tab bar position
- Fix parsing arguments

# v1.3.1
- GUI improvements
- Mode for Vi navigation (h, j, k, l keys for movement)
- Better performance

# v1.3.0
- Import/export items to/from a file (not compatible with older saved format)
- Use scripts to handle item history
- Improved performance

# v1.2.5
- Save/load items to/from a file
- Sort selected items
- Easier tab browsing (left/right arrow keys)
- GUI improvements
- More shortcut combinations work on Linux

# v1.2.4
- Improved commands
- Fixed and faster scrolling
- Better tab manipulation

# v1.2.3
- Bugfixes and major clean-up

# v1.2.2
- Performance improved

# v1.2.1
- Save items from commands in other tabs
- Missing icons in Windows version

# v1.2.0
- Appearance settings
- Tab manipulation from command line
- Copy/paste items from/to tabs
- Faster searching

# v1.1.0
- Better performance
- New configuration options
- Improved command line

# v1.0.2
- Improved Windows compatibility
- Global shortcuts
- Automatic commands

# v1.0.1
- Compatibility with different platforms


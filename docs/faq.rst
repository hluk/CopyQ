FAQ - Frequently Asked Questions
================================

.. _faq-show-app:

How to open CopyQ window or tray menu using shortcut?
-----------------------------------------------------

Add new command to open the CopyQ window or menu with a global shortcut:

1. Open "Command" dialog (``F6`` shortcut).
2. Click "Add" button in the dialog.
3. Select "Show/hide main window" or "Show the tray menu" from the list
   and click "OK" button.
4. Click the button next to "Global Shortcut" label and set the
   shortcut.
5. Click "OK" button to save the changes.

For more information about commands see :ref:`writing-commands`.

.. _faq-paste-from-window:

How to paste double-clicked item from CopyQ window?
---------------------------------------------------

1. Open "Preferences" (``Ctrl+P`` shortcut).
2. Go to "History" tab.
3. Enable "Paste to current window" option.

Next time you open the CopyQ main window and activate an item,
it should be pasted.

.. _faq-paste-text:

How to paste as plain text?
---------------------------

To **paste clipboard as plain text**:

1. Open "Command" dialog (``F6`` shortcut).
2. Click "Add" button in the dialog.
3. Select "Paste clipboard as plain text" from the list and click "OK" button.
4. Click the button next to "Global Shortcut" label and set the shortcut.
5. Click "OK" button to save the changes.

To **paste selected items as plain text** (from CopyQ window) follow the steps above
but add "Paste as Plain Text" command instead and change "Shortcut".

.. _faq-store-text:

How to store only plain text?
-----------------------------

To **disallow storing HTML and rich text**:

1. Open "Preferences" (``Ctrl+P`` shortcut).
2. Go to "Items" tab.
3. Disable "Web" item in the list.
4. Select "Text" item.
5. Disable "Save and display HTML and rich text".

Similarly, you can also disable "Images" in the list to avoid storing and
rendering images.

Existing items won't be affected but **any data formats can be removed**:

1. Select an item.
2. Press ``F4`` shortcut ("Item - Show Content..." in menu).
3. Select format from list.
4. Press ``Delete`` key.

.. _faq-disable-clipboard-storing:

How to disable storing clipboard?
---------------------------------

To temporarily disable storing the clipboard in the CopyQ item list,
select menu item "File - Disable Clipboard Storing" (``Ctrl+Shift+X`` shortcut).
To re-enable the functionality select "File - Enable Clipboard Storing" (same shortcut).

To permanently disable storing the clipboard in CopyQ:

1. Open "Preferences" (``Ctrl+P`` shortcut).
2. Go to "History" tab.
3. Clear "Tab for storing clipboard" field.

How to back up tabs, configuration and commands?
------------------------------------------------

From menu select "File - Export" and choose which tabs to export and whether to export
configuration and commands.

To restore the backup, select menu item "File - Import", select the exported file, and
then choose what to import back.

.. note::

   Importing tabs and commands won't override existing tabs, and will create new ones.

See also: :ref:`backup`

.. _faq-disable-notifications:

How to enable or disable displaying notification when clipboard changes?
------------------------------------------------------------------------

To enable displaying the notifications:

1. Open "Preferences" (``Ctrl+P`` shortcut).
2. Go to "Notifications" tab.
3. Set non-zero value for "Interval in seconds to display notifications".
4. Set non-zero value for "Number of lines for clipboard notification".
5. Click "OK" button.

To enable displaying the notifications, set either of the options
mentioned above to zero.

.. _faq-share-commands:

How to load shared commands and share them?
-------------------------------------------

You can stumble upon code that looks like this.

.. code-block:: ini

    [Command]
    Name=Show/hide main window
    Command=copyq: toggle()
    Icon=\xf022
    GlobalShortcut=ctrl+shift+1

This code represents a command that can used in CopyQ (specifically it
opens main window on Ctrl+Shift+1). To use the command in CopyQ:

1. Copy the code above.
2. Open "Command" dialog (``F6`` shortcut).
3. Click "Paste Commands" button at the bottom of the dialog.
4. Click OK button.

(Now you should be able to open main window with Ctrl+Shift+1.)

To share your commands, you can select the commands from command list in
"Command" dialog and press "Copy Selected" button (or just hit Ctrl+C).

.. _faq-ignore-password-manager:

How to omit storing text copied from specific windows like a password manager?
------------------------------------------------------------------------------

Add and modify automatic command to ignore text copied from the window:

1. Open "Command" dialog (``F6`` shortcut).
2. Click "Add" button in the dialog.
3. Select "Ignore *Password* window" from the list and click "OK"
   Button.
4. Select "Show Advanced"
5. Change "Window" text box to match the title (or part of it) of the
   Window to ignore (e.g. ``KeePass``). But for **KeePassXC** (and possibly
   other apps), it is better to set "Format" to ``x-kde-passwordManagerHint``
   instead (also remember to remove the default that is set in the "Window"
   setting, since both "Window" and "Format" need to match if they're set).
6. Click "OK" button to save the changes.

.. note::

    This new command should be at the top of the command list because
    automatic commands are executed in the order they appear in the list,
    and we don't want to process sensitive data in any way.

In some cases, e.g. the password manager is an extension of a web browser or a
password is copied from a menu instead of a window, the command above won't
work. You can try setting the "Window" text box to ``^$``, which usually matches
popup menus.

For a more reliable way, use `a command to blacklist texts
<https://github.com/hluk/copyq-commands/tree/master/Scripts#blacklisted-texts>`__
(it stores just a salted hash, the text itself is not stored anywhere).

.. _faq-logging:

How to enable logging?
----------------------

Set environment variable ``COPYQ_LOG_LEVEL`` to ``DEBUG`` for verbose logging
and set ``COPYQ_LOG_FILE`` to a file path for the log.

You can copy current log file path to clipboard from Action dialog (F5 shortcut)
by entering command ``copyq 'copy(info("log"))'``. Alternatively, press ``F12``
to directly access the log.

If you **cannot access GUI**, you can **restart CopyQ from terminal** and **log
to a separate file**. On Linux and macOS:

.. code-block:: zsh

    copyq exit
    export COPYQ_LOG_LEVEL='DEBUG'
    export COPYQ_LOG_FILE="$HOME/copyq.log"
    echo "Logs will be written to $COPYQ_LOG_FILE"
    copyq

On Windows (in PowerShell):

.. code-block:: powershell

    & 'C:\Program Files\CopyQ\copyq.exe' exit
    $env:COPYQ_LOG_LEVEL = 'DEBUG'
    $env:COPYQ_LOG_FILE = [Environment]::GetFolderPath("MyDocuments") + '\copyq.log'
    echo "Logs will be written to $env:COPYQ_LOG_FILE"
    & 'C:\Program Files\CopyQ\copyq.exe'

How to preserve the order of copied items when copying or pasting multiple items?
---------------------------------------------------------------------------------

a. Reverse order of selected items with ``Ctrl+Shift+R`` and copy them.
b. Alternatively, select items in reverse order and then copy.

See `#165 <https://github.com/hluk/CopyQ/issues/165#issuecomment-34745058>`__.

How does pasting single/multiple items work internally?
-------------------------------------------------------

``Return`` key copies the whole item (with all formats) to the clipboard
and -- if the "Paste to current window" option is enabled -- it sends
``Shift+Insert`` to previous window. So the target application decides
what format to paste on ``Shift+Insert``.

If you select more items and press ``Return``, just the concatenated
text of selected items is put into the clipboard. Though it could do more
in future, like join HTML, images or other formats.

See `#165 <https://github.com/hluk/CopyQ/issues/165#issuecomment-34957089>`__.

Why does pasting from CopyQ not work?
-------------------------------------

Pasting from CopyQ works only on Windows, macOS and X11 on Linux.

Specifically, this feature is not supported on Wayland, but you can use
the workaround: :ref:`known-issue-wayland`

First, check if you have the appropriate options enabled:

a. For pasting from main window, enable "Paste to current window" in "History"
   configuration tab.
b. For pasting from tray menu, enable "Paste activated item to current window"
   in "Tray" configuration tab.

If the pasting still doesn't work, check if ``Shift+Insert`` shortcut pastes to
the target window. That's the shortcut CopyQ uses by default. To change this to
``Ctrl+V`` see `#633
<https://github.com/hluk/CopyQ/issues/633#issuecomment-278326916>`__.

If pasting still doesn't work, it could be caused by either of these problems:

- CopyQ fails to focus the target window correctly.
- The format copied to the clipboard is not supported by the target application.

How to open the menu or context menu with only the keyboard?
------------------------------------------------------------

Use ``Alt+I`` to open the item menu or use the ``Menu`` key on your keyboard
to open the context menu for selected items.

.. _faq-hide-menu-bar:

How to hide the menu bar in the main CopyQ window?
--------------------------------------------------

The menu bar can be hidden by modifying the style sheet of the current theme.

1. Open "Preferences" (``Ctrl+P`` shortcut).
2. Go to "Appearance" tab.
3. Enable checkbox "Set colors for tabs, tool bar and menus".
4. Click "Edit Theme" button.
5. Find ``menu_bar_css`` option and add ``height: 0``:

.. code-block:: ini

    menu_bar_css="
        ;height: 0
        ;background: ${bg}
        ;color: ${fg}"

How to reuse file paths copied from a file manager?
---------------------------------------------------

By default, only the text is stored in item list when you copy or cut
files from a file manager. Other data is usually needed to be able to
copy/paste files from CopyQ.

You have to add additional data formats (MIME) using an automatic command
(similar to one below). The commonly used format in many file managers is
``text/uri-list``. Other special formats include
``x-special/gnome-copied-files`` for Nautilus and
``application/x-kde-cutselection`` for Dolphin. These formats are used to
specify type of action (copy or cut).

.. code-block:: ini

    [Command]
    Command="
        var originalFunction = global.clipboardFormatsToSave
        global.clipboardFormatsToSave = function() {
            return originalFunction().concat([
                mimeUriList,
                'x-special/gnome-copied-files',
                'application/x-kde-cutselection',
            ])
        }"
    Icon=\xf0c1
    IsScript=true
    Name=Store File Manager Metadata

How to trigger a command based on primary selection only?
---------------------------------------------------------

You can check ``application/x-copyq-clipboard-mode`` format in automatic commands.

E.g. if you set input format of a command it will be only executed on Linux
mouse selection change:

.. code-block:: ini

    [Command]
    Automatic=true
    Command="
        copyq:
        popup(input())"
    Input=application/x-copyq-clipboard-mode
    Name=Executed only on X11 selection change

Otherwise you can check it in command:

.. code-block:: ini

    [Command]
    Automatic=true
    Command="
        copyq:
        if (str(data(mimeClipboardMode)) == 'selection')
          popup('selection changed')
        else
          popup('clipboard changed')"
    Name=Show clipboard/selection change

Why can I no longer paste from the application on macOS?
--------------------------------------------------------

See: :ref:`known-issue-macos-paste-after-install`

Why does my external editor fail to edit items?
-----------------------------------------------

CopyQ creates a temporary file with content of the edited item and passes it as
argument to the custom editor command. If the file changes, the item is also
modified.

Usual issues are:

- External editor opens an empty file.
- External editor warns that the file is missing.
- Saving the file doesn't have any effect on the origin item.

This happens if **the command to launch the editor exits, but the editor
application itself is still running**. Since the command exited, CopyQ assumes
that the editor itself is no longer running, and stops monitoring the changes
in the temporary file (and removes the file).

Here is the correct command to use for some editors::

    emacsclientw.exe --alternate-editor="" %1
    gvim --nofork %1
    sublime_text --wait %1
    code --wait %1
    open -t -W -n %1

.. _faq-config-path:

Where to find saved items and configuration?
--------------------------------------------

You can find configuration and saved items in:

a. Windows folder ``%APPDATA%\copyq`` for installed version of CopyQ.
b. Windows sub-folder ``config`` in unzipped portable version of CopyQ.
c. Linux directory ``~/.config/copyq``.
d. In a directory specific to a given CopyQ instance - see :ref:`sessions`.

Run ``copyq info config`` to get absolute path to the configuration file
(parent directory contains saved items).

Why are items and configuration not saved?
------------------------------------------

Check access rights to configuration directory and files.

Why do global shortcuts not work?
---------------------------------

Global/system shortcuts (or specific key combinations) don't work in some
desktop environments (e.g. Wayland on Linux).

As a workaround, you can try to assign the shortcuts in your system settings.

To get the command to launch for a shortcut:

1. Open Command dialog (F6 from main window).
2. Click on the command with the global shortcut in the left panel.
3. Enable "Show Advanced" checkbox.
4. Copy the content of "Command" text field.

.. note::

   If the command looks like this:

   ::

      copyq: toggle()

   the actual command to use is:

   ::

      copyq -e "toggle()"

.. _faq-force-hide-main-window:

Why doesn't the main window close on tiling window managers?
------------------------------------------------------------

The main window remains open if it cannot minimize to task bar and tray icon is
not available. This is a safety feature to allow users to see that the
application is running and make any initial setup when the app is started for
the first time.

To force hiding main window:

1. Open "Preferences" (``Ctrl+P`` shortcut).
2. Go to "Layout" tab.
3. Enable "Hide main window" option.

Alternatively, run the following command::

    copyq config hide_main_window true

Why does encryption ask for password so often?
----------------------------------------------

Encryption plugin uses ``gpg2`` to decrypt tabs and items. The password usually
needs to be entered only once every few minutes.

If the password prompt is showing up too often, either increase tab unloading
interval ("Unload tab after an interval" option in "History" tab in
Preferences), or change ``gpg`` configuration (see `#946
<https://github.com/hluk/CopyQ/issues/946#issuecomment-389538964>`__).

How to fix "copyq: command not found" errors?
---------------------------------------------

If you're getting ``copyq: command not found`` or similar error, it means that
the ``copyq`` executable cannot be found by the shell or a language interpreter.

This usually happens if the executable's directory is not in the ``PATH``
environmental variable.

If this happens when running from within the command, e.g.

.. code-block:: bash

    bash:
    text="SOME TEXT"
    copyq copy "$text"

you can **fix it by using** ``COPYQ`` environment variable instead.

.. code-block:: bash

    bash:
    text="SOME TEXT"
    "$COPYQ" copy "$text"

What to do when CopyQ crashes or misbehaves?
--------------------------------------------

When CopyQ crashes or doesn't behave as expected, try to look up
a similar `issue <https://github.com/hluk/CopyQ/issues>`__ first
and provide details in a comment for that issue.

If you cannot find any such an issue, `report a new bug
<https://github.com/hluk/CopyQ/issues/new>`__.

Try to provide the following details:

- CopyQ version
- operating system (desktop environment, window manager, etc.)
- steps to reproduce the issue
- application log (see :ref:`faq-share-commands`)
- stacktrace if available (e.g. on Linux ``coredumpctl dump --reverse copyq``)

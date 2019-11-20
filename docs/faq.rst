FAQ - Frequently Asked Questions
================================

.. _faq-show-app:

How to open application window or tray menu using shortcut?
-----------------------------------------------------------

Add new command to open window or menu with global shortcut:

1. open "Command" dialog (``F6`` shortcut),
2. click "Add" button in the dialog,
3. select "Show/hide main window" or "Show the tray menu" from the list
   and click "OK" button,
4. click the button next to "Global Shortcut" label and set the
   shortcut,
5. click "OK" button to save the changes.

For more information about commands see :ref:`writing-commands`.

.. _faq-paste-from-window:

How to paste double-clicked item from application window?
---------------------------------------------------------

1. Open "Preferences" (``Ctrl+P`` shortcut),
2. go to "History" tab,
3. enable "Paste to current window" option.

Next time you open main window and activate an item it should be pasted.

.. _faq-paste-text:

How to paste as plain text?
---------------------------

To **paste clipboard as plain text**:

1. open "Command" dialog (``F6`` shortcut),
2. click "Add" button in the dialog,
3. select "Paste clipboard as plain text" from the list and click "OK" button,
4. click the button next to "Global Shortcut" label and set the shortcut,
5. click "OK" button to save the changes.

To **paste selected items as plain text** (from application window) follow the steps above
but add "Paste as Plain Text" command instead and change "Shortcut".

.. _faq-store-text:

How to store only plain text?
-----------------------------

To **disallow storing HTML and rich text**:

1. Open "Preferences" (``Ctrl+P`` shortcut),
2. go to "Items" tab,
3. disable "Web" item in the list,
4. select "Text" item,
5. and disable "Save and display HTML and rich text".

Similarly you can also disable "Images" in the list to avoid storing and
rendering images.

Existing items won't be affected but **any data formats can be removed**:

1. Select an item,
2. press ``F4`` shortcut ("Item - Show Content..." in menu),
3. select format from list,
4. press ``Delete`` key.

.. _faq-disable-clipboard-storing:

How to disable storing clipboard?
---------------------------------

To temporarily disable storing clipboard in item list,
select menu item "File - Disable Clipboard Storing" (``Ctrl+Shift+X`` shortcut).
To re-enable the functionality select "File - Enable Clipboard Storing" (same shortcut).

To permanently disable storing clipboard:

1. Open "Preferences" (``Ctrl+P`` shortcut),
2. go to "History" tab,
3. clear "Tab for storing clipboard" field.

How to back up tabs, configuration and commands?
------------------------------------------------

From menu select "File - Export" and choose what tabs to export and whether to export
configuration and commands.

To restore the backup select menu item "File - Import", select the exported file and
choose what to import back.

.. note::

   Importing tabs and commands won't override existing tabs but create new ones.

How to enable or disable displaying notification when clipboard changes?
------------------------------------------------------------------------

To enable displaying the notifications:

1. open "Preferences" (``Ctrl+P`` shortcut),
2. go to "Notifications" tab,
3. set non-zero value for "Interval in seconds to display notifications",
4. set non-zero value for "Number of lines for clipboard notification",
5. click "OK" button.

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

1. copy the code above,
2. open "Command" dialog (``F6`` shortcut),
3. click "Paste Commands" button at the bottom of the dialog,
4. click OK button.

(Now you should be able to open main window with Ctrl+Shift+1.)

To share your commands, you can select the commands from command list in
"Command" dialog and press "Copy Selected" button (or just hit Ctrl+C).

How to omit storing text copied from specific windows like a password manager?
------------------------------------------------------------------------------

Add and modify automatic command to ignore text copied from the window:

1. open "Command" dialog (``F6`` shortcut),
2. click "Add" button in the dialog,
3. select "Ignore *Password* window" from the list and click "OK"
   button,
4. select "Show Advanced"
5. change "Window" text box to match the title (or part of it) of the
   window to ignore (e.g. ``KeePass``),
6. click "OK" button to save the changes.

.. note::

    This new command should be at top of the command list because
    automatic commands are executed in order they appear in the list and we
    don't want to process sensitive data in any way.

In some cases, e.g. the password manager is an extension of a web browser or a
password is copied from a menu instead of a window, the command above won't
work. You can try setting "Window" text box to ``^$`` which usually matches
popup menus.

For more reliable way, use `a command to blacklist texts
<https://github.com/hluk/copyq-commands/tree/master/Scripts#blacklisted-texts>`__
(it stores just a salted hash, the text itself is not stored anywhere).

.. _faq-logging:

How to enable logging?
----------------------

Set environment variable ``COPYQ_LOG_LEVEL`` to ``DEBUG`` for verbose logging
and set ``COPYQ_LOG_FILE`` to a file path for the log.

You can copy current log file path to clipboard from Action dialog (F5 shortcut)
by entering command ``copyq 'copy(info("log"))'``. Alternatively, press ``F12`` to directly access the log.

How to preserve the order of copied items on copy or pasting multiple items?
----------------------------------------------------------------------------

a. Reverse order of selected items with ``Ctrl+Shift+R`` and copy them or
b. select items in reverse order and copy.

See `#165 <https://github.com/hluk/CopyQ/issues/165#issuecomment-34745058>`__.

How does pasting single/multiple items internally work?
-------------------------------------------------------

``Return`` key copies the whole item (with all formats) to the clipboard
and -- if the "Paste to current window" option is enabled -- it sends
``Shift+Insert`` to previous window. So the target application decides
what format to paste on ``Shift+Insert``.

If you select more items and press ``Return``, just the concatenated
text of selected items is put into clipboard. Thought it could do more
in future, like join HTML, images or other formats.

See `#165 <https://github.com/hluk/CopyQ/issues/165#issuecomment-34957089>`__.

How to open the menu or context menu with only the keyboard?
------------------------------------------------------------

Use ``Alt+I`` to open the item menu or use the ``Menu`` key on your keyboard
to open the context menu for selected items.

How to hide menu bar in main window?
------------------------------------

Menu bar can be hidden by modifying style sheet of current theme.

1. Open "Preferences" (``Ctrl+P`` shortcut),
2. go to "Appearance" tab,
3. enable checkbox "Set colors for tabs, tool bar and menus",
4. click "Edit Theme" button,
5. find ``menu_bar_css`` option and add ``height: 0``:

.. code-block:: ini

    menu_bar_css="
        ;height: 0
        ;background: ${bg}
        ;color: ${fg}"

How to reuse file paths copied from a file manager?
---------------------------------------------------

By default only the text is stored in item list when you copy of cut
files from a file manager. Other data are usually needed to be able to
copy/paste files from CopyQ.

You have to add additional data formats (MIME) using an automatic command
(similar to one below). Commonly used format in many file managers is
``text/uri-list``. Other special formats include
``x-special/gnome-copied-files`` for Nautilus,
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

E.g. if you set input format of a command it would be only executed on X11 selection change:

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

To fix this you can try following steps.

1. Go to System Preferences -> Security & Privacy -> Privacy -> Accessibility
   (or just search for "Allow apps to use Accessibility"),
2. Click the unlock button,
3. Select CopyQ from the list and remove it (with the "-" button).

See also Issues `#1030 <https://github.com/hluk/CopyQ/issues/1030>`__ and `#1245 <https://github.com/hluk/CopyQ/issues/1245>`__.

Why does my external editor fail to edit items?
-----------------------------------------------

CopyQ creates a temporary file with content of the edited item and passes it as
argument to custom editor command. If the file changes, the item is also
modified.

Usual issues are:

- external editor opens an empty file,
- external editor warns that the file is missing or
- saving the file doesn't have any effect on the origin item.

This happens if **the command to launch editor exits but the editor
application itself is still running**. Since the command exited, CopyQ assumes
that the editor itself is no longer running and stops monitoring the changes in
temporary file (and removes the file).

Here is the correct command to use for some editors::

    emacsclientw.exe --alternate-editor="" %1
    gvim --nofork %1
    sublime_text --wait %1
    code --wait %1
    open -t -W -n %1

Where to find saved items and configuration?
--------------------------------------------

You can find configuration and saved items in:

- Windows folder ``%APPDATA%\copyq`` for installed version of the app or ``config``
  folder in unzipped portable version,
- Linux directory ``~/.config/copyq``.

Run ``copyq info config`` to get absolute path to the configuration file
(parent directory contains saved items).

.. note::

   Main configuration for installed version of the app on Windows is stored in registry.

Why are items and configuration not saved?
------------------------------------------

Check access rights to configuration directory and files.

Why global shortcuts don't work?
--------------------------------

Global/system shortcuts (or specific key combinations) don't work in some desktop environments (e.g. Wayland on Linux).

As a workaround, you can try to assign the shortcuts in your system settings.

To get the command to launch for a shortcut:

1. open Command dialog (F6 from main window),
2. in left panel, click on the command with the global shortcut,
3. enable "Show Advanced" checkbox,
4. copy the content of "Command" text field.

.. note::

   If the command looks like this:

   ::

      copyq: toggle()

   the actual command to use is:

   ::

      copyq -e "toggle()"

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
``copyq`` executable cannot be found by the shell or a language interpreter.

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

What to do when application crashes or misbehaves?
--------------------------------------------------

When the application crashes or doesn't behave as expected, try to look up
similar `issue <https://github.com/hluk/CopyQ/issues>`__ first and provide
details in a comment.

If you cannot find any such issue, `report a new bug
<https://github.com/hluk/CopyQ/issues/new>`__.

Try to provide following detail.

1. Application version
2. Operating System (desktop environment, window manager etc.)
3. Steps to reproduce the issue.
4. Application log (see :ref:`faq-share-commands`)
5. Back trace if available (e.g. on Linux ``coredumpctl dump --reverse copyq``)

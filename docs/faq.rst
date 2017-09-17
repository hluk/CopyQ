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

You can also disallow rich text storing: go to preferences,
"Items" tab and uncheck "Web" checkbox under "Text" uncheck "HTML" checkbox.

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

   Importing tabs and commands won't override existing tabs but create new ones

How to enable or disable displying notification when clipboard changes?
-----------------------------------------------------------------------

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

How to enable logging
---------------------

Set environment variable ``COPYQ_LOG_LEVEL`` to ``DEBUG`` for verbose logging
and set ``COPYQ_LOG_FILE`` to a file path for the log.

You can copy current log file path to clipboard from Action dialog (F5 shortcut)
by entering command ``copyq 'copy(info("log"))'``.

Alternatively, press `F12` to directly access the log or find your log files `~/.local/share/copyq/copyq/copyq.log`.

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

Is it possible to hide menu bar to have even cleaner main window?
-----------------------------------------------------------------

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

You have to add new data formats (MIME) to format list in "Data" item
under "Item" configuration tab. Commonly used format in many file
managers is ``text/uri-list``. Other special formats include
``x-special/gnome-copied-files`` for Nautilus,
``application/x-kde-cutselection`` for Dolphin. These formats are used
to specify type of action (copy or cut).

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

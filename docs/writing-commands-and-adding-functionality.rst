.. _writing-commands:

Writing Commands and Adding Functionality
=========================================

CopyQ allows you to extend its functionality through commands in
following ways:

- Add custom commands to context menu for selected items in history.
- Run custom commands automatically when clipboard changes.
- Assign global/system-wide shortcuts to custom commands.

Here are some examples what can be achieved by using commands:

-  Automatically store web links or other types of clipboard content in
   special tabs to keep the history clean.
-  Paste current date and time or modified clipboard on a global
   shortcut.
-  Pass selected items or clipboard to external application (e.g. web
   browser or image editor).
-  Keep TODO lists and tag items as "important" or use custom tags.
-  See :ref:`command-examples` for some other ideas and useful commands.

Command Dialog
--------------

You can create new commands in Command dialog. To open the dialog
either:

a. Press default shortcut F6.
b. Select menu item "Commands/Global Shortcuts..." in "File" menu.

Command dialog contains:

- list of custom commands on the left
- settings for currently selected command on the right
- command filter text field at the top
- buttons to modify the command list (add, remove and move commands) at the top
- buttons to save, load, copy and paste commands at the bottom

Create New Command
~~~~~~~~~~~~~~~~~~

To create new command click the "Add" button in Command dialog. This
opens list with predefined commands.

"New Command" creates new empty command (but it won't do anything
without being configured). One of the most frequently used predefined
command is "Show/hide main window" which allows you to assign global
shortcut for showing and hiding CopyQ window.

If you double click a predefined command (or select one or multiple
commands and click OK) it will be added to list of commands. The right
part of the Command dialog now shows the configuration for the new
command.

For example, for the "Show/hide main window" you'll most likely need to
change only the "Global Shortcut" option so click on the button next to
it and press the shortcut you want to assign.

Commands can be quickly disabled by clicking the check box next to them
in command list.

By clicking on "OK" or "Apply" button in the dialog all commands will be
saved permanently.

Command Options
^^^^^^^^^^^^^^^

The following options can be set for commands.

If unsure what an option does, hover mouse pointer over it and tool tip
with description will appear.

Name
''''

Name of the command. This is used in context menu if "In Menu" check box
is enabled. Use the pipe character ``|`` in the name to create sub-menus.

Group: Type of Action
'''''''''''''''''''''

This group sets the main type of the command. Usually only one
sub-option is set.

Automatic
"""""""""

If enabled, the command is triggered whenever clipboard changes.

Automatic items are run in order they appear in the command list. No
other automatic commands will be run if a triggered automatic command
has "Remove Item" option set or calls ``copyq ignore``.

The command is **applied on current clipboard data** - i.e. options
below access text or other data in clipboard.

In Menu
"""""""

If enabled, the command can be run from main window either with
application shortcut, from context menu or "Item" menu. The command can
be also run from tray menu.

Shortcuts can be assigned by clicking on the button next to the option.
These **application shortcuts work only while CopyQ window has focus**.

If the command is run from **tray menu**, it is **applied on clipboard
data**, otherwise it's **applied on data in selected items**.

Global Shortcut
"""""""""""""""

Global or system shortcut is a keyboard shortcut that **works even if the main
application window is not focused**.

If enabled, the command is triggered whenever assigned shortcut is pressed.

This command is **not applied on data** in clipboard nor selected items.

.. _command-dialog-script:

Script
""""""

If enabled, the command is script which is loaded before any other script is
started. This allows overriding existing functions and creating new ones
(allowing new command line arguments to be used).

See :ref:`commands-script`.

.. _command-dialog-display:

Display
"""""""

If enabled, the command is used to modify item data before displaying. Use
``data()`` to retrieve current item data and ``setData()`` to modify the data
to display (these are not stored permanently).

See :ref:`commands-display`.

Group: Match Items
''''''''''''''''''

This group is visible only for "Automatic" or "In Menu" commands.
Sub-options specify when the command can be used.

1. Content
""""""""""

`Regular expression <https://doc.qt.io/qt-4.8/qregexp.html#introduction>`__
to match text of selected items (for "In Menu" command) or clipboard
(for "Automatic" command).

For example, ``^https?://`` will match simple web addresses (text
starting with ``http://`` or ``https://``).

2. Window
"""""""""

`Regular expression <https://doc.qt.io/qt-4.8/qregexp.html#introduction>`__
to match window title of active window (only for "Automatic" command).

For example, ``- Chromium$`` or ``Mozilla Firefox$`` to match some web
browser window titles (``$`` in the expression means end of the title).

3. Filter
"""""""""

A command for validating text of selected items (for "In Menu" command)
or clipboard (for "Automatic" command).

If the command exits with non-zero exit code it won't be shown in
context menu and automatically triggered on clipboard change.

Example, ``copyq: if (tab().indexOf("Web") == -1) fail()`` triggers the
command only if tab "Web" is available.

4. Format
"""""""""

Match format of selected items or clipboard.

The data of this format will be sent to **standard input** of the
command - this doesn't apply if the command is triggered with global
shortcut.

Command
'''''''

The command to run.

This can contain either:

a. simple command line (e.g. ``copyq popup %1`` - expression ``%1`` means text of the selected item or clipboard)
b. input for command interpreter (prefixed with ``bash:``, ``powershell:``, ``python:`` etc.)
c. CopyQ script (prefixed with ``copyq:``)

You can use ``COPYQ`` environment variable to get path of application
binary.

Current CopyQ session name is stored in ``COPYQ_SESSION_NAME``
environment variable (see :ref:`sessions`).

Example (call CopyQ from Python):

.. code-block:: python

    python:
    import os
    from subprocess import call
    copyq = os.environ['COPYQ']
    call([copyq, 'read', '0'])

Example (call CopyQ from PowerShell on Windows):

::

    powershell:
    $Item1 = (& "$env:COPYQ" read 0 | Out-String)
    echo "First item: $Item1"

Group: Action
'''''''''''''

This group is visible only for "Automatic" or "In Menu" commands.

1. Copy to tab
""""""""""""""

Creates new item in given tab.

2. Remove Item
""""""""""""""

Removes selected items. If enabled for "Automatic" command, the
clipboard will be ignored and no other automatic commands will be
executed.

Group: Menu Action
''''''''''''''''''

This group is visible only for "In Menu" commands.

1. Hide main window after activation
""""""""""""""""""""""""""""""""""""

If enabled, main window will be hidden after the command is executed.

Group: Command options
''''''''''''''''''''''

This group is visible only for "Automatic" or "In Menu" commands.

1. Wait
"""""""

Show action dialog before applying options below.

2. Transform
""""""""""""

Modify selected items - i.e. remove them and replace with **standard
output** of the command.

3. Output
"""""""""

Format of **standard output** to save as new item.

4. Separator
""""""""""""

Separator for splitting output to multiple items (``\n`` to split
lines).

5. Output tab
"""""""""""""

Tab for saving the output of command.

Save and Share Commands
~~~~~~~~~~~~~~~~~~~~~~~

You can back up or share commands by saving them in a file ("Save
Selected Commands..." button) or by copying them to clipboard.

The saved commands can be loaded back to command list ("Load
Commands..." button) or pasted to the list from clipboard.

You can try some examples by copying commands from :ref:`command-examples`.

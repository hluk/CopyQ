.. _backup:

Backup
======

This page describes how to back up tabs, configuration and commands in
CopyQ.

Back Up All Data Automatically on Exit
--------------------------------------

You can use `command that backs up all items, tabs and settings after exit
<https://github.com/hluk/copyq-commands/tree/master/Scripts#backup-on-exit>`__.

To install the command see `the description in the repository
<https://github.com/hluk/copyq-commands/blob/master/README.md>`__.

Back Up Manually
----------------

To back up all the data, **exit the application** first and **copy
the configuration directory** and **the data directory**.

Path to the configuration is usually:

-  Windows: ``%APPDATA%\copyq``
-  Portable version for Windows: ``config`` sub-folder in unzipped
   application directory
-  Linux: ``~/.config/copyq``

Path to the data is usually:

-  Windows: ``%APPDATA%\copyq\items``
-  Portable version for Windows: ``items`` sub-folder in unzipped
   application directory
-  Linux: ``~/.local/share/copyq/copyq/items``

To copy the configuration path to clipboard from CopyQ:

1. Open Action dialog (``F5`` shortcut).
2. Enter the command:

.. code-block:: js

    copyq:
    dir = Dir(info('config') + '/..')
    copy(dir.absolutePath())

3. Click OK dialog button.

To copy the data path, change ``'config'`` to ``'data'``.

To restore the backup, exit the application and replace the
configuration directory.

.. warning::

    Before making or restoring back up, always exit CopyQ
    (don't only close the main window).

Export and Import
-----------------

You can easily export selected tabs and optionally
configuration and commands within the application.

.. warning::

    Tabs are always exported **unencrypted** and if a tab is
    synchronized with directory on disk the files themselves won't be
    exported.

To export the data click "Export..." in "File" menu and select what to
export, confirm with OK button and select file to save the stuff to.

To restore the data click "Import..." in "File" menu, select file to
import and select what to import.

.. note::

    Import won't overwrite existing tabs and commands but create new ones.

Alternatively you can use command line for export and import everything
(selection dialogs won't be opened).

.. code-block:: bash

    copyq exportData {FILE/PATH/TO/EXPORT}
    copyq importData {FILE/PATH/TO/IMPORT}

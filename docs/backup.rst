Backup
======

This page describes how to back up tabs, configuration and commands in
CopyQ.

Back Up Manually
----------------

To back up all the data, close the application first and copy
configuration directory.

Path to configuration can be retrieved by running following command.

.. code-block:: bash

    copyq info config

-  Windows: ``%APPDATA%\copyq``
-  Portable version for Windows: ``config`` sub-folder in unzipped
   application directory
-  Linux: ``~/.config/copyq``

To restore the backup, close the application and replace the
configuration directory.

Export and Import
-----------------

In CopyQ 3.0.0 you can easily export selected tabs and optionally
configuration and commands within the application.

**Important:** Tabs are always exported **unencrypted** and if a tab is
synchronized with directory on disk the files themselves won't be
exported.

To export the data click "Export..." in "File" menu and select what to
export, confirm with OK button and select file to save the stuff to.

To restore the data click "Import..." in "File" menu, select file to
import and select what to import.

Import won't overwrite existing tabs commands but create new ones.

Alternatively you can use command line for export and import everything
(selection dialogs won't be opened).

.. code-block:: bash

    copyq exportData {FILE/PATH/TO/EXPORT}
    copyq importData {FILE/PATH/TO/IMPORT}

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

To back up all the data, **exit the application** first and copy
the configuration, data and state directories.

.. versionchanged:: 17.0.0
   Tab data and state files were previously stored in the configuration
   directory. They are migrated automatically on first startup.

Default paths since version 17.0.0:

.. list-table::
   :header-rows: 1

   * - Platform
     - Configuration
     - Data
     - State
   * - Linux
     - ``~/.config/copyq``
     - ``~/.local/share/copyq``
     - ``~/.local/state/copyq``
   * - macOS
     - ``~/Library/Preferences/copyq``
     - ``~/Library/Application Support/copyq``
     - ``~/Library/Application Support/copyq``
   * - Windows
     - ``%APPDATA%\copyq``
     - ``%LOCALAPPDATA%\copyq``
     - ``%LOCALAPPDATA%\copyq``
   * - Windows (portable)
     - ``config``
     - ``config``
     - ``config``

Default paths before version 17.0.0:

.. list-table::
   :header-rows: 1

   * - Platform
     - Configuration, tab data, state
     - Item data
     - Logs
   * - Linux
     - ``~/.config/copyq``
     - ``~/.local/share/copyq/items``
     - ``~/.local/share/copyq``
   * - macOS
     - ``~/Library/Preferences/copyq``
     - ``~/Library/Application Support/copyq/items``
     - ``~/Library/Application Support/copyq``
   * - Windows
     - ``%APPDATA%\copyq``
     - ``%APPDATA%\copyq\items``
     - ``%APPDATA%\copyq``
   * - Windows (portable)
     - ``config``
     - ``config\items``
     - ``logs``

To copy a directory path to clipboard:

.. code-block:: js

    copyq:
    copy(Dir(info('config') + '/..').absolutePath())

Replace ``'config'`` with ``'data'`` or ``'state'`` for the other directories.

To restore the backup, exit the application and replace the
directories.

.. warning::

    Before making or restoring back up, always exit CopyQ
    (don't only close the main window).

Export and Import
-----------------

You can easily export selected tabs and optionally
configuration and commands within the application.

.. warning::

    Exported data is **not encrypted by default**. When exporting from the GUI,
    CopyQ prompts for an optional export password — if provided, tab data in the
    export file is encrypted with that password. The ``exportData`` script
    command does not support this and always exports unencrypted. If a tab is
    synchronized with a directory on disk, the files themselves are not exported.

To export the data click "Export..." in "File" menu, select what to
export, confirm with OK button and select target file to save.

To restore the data click "Import..." in "File" menu, select file to
import and select what to import.

.. note::

    Import will not overwrite existing tabs and commands but create new ones.

Alternatively you can use command line for export and import everything
(selection dialogs won't be opened).

.. code-block:: bash

    copyq exportData {FILE/PATH/TO/EXPORT}
    copyq importData {FILE/PATH/TO/IMPORT}

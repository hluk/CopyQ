.. _sessions:

Sessions
========

You can run multiple instances of the application given that they have
different session names.

Running Multiple Instances
--------------------------

Each application instance should have unique name.

To start new instance with ``test1`` name, run:

::

    copyq --session=test1

This instance uses configuration, tabs and items unique to given session
name.

You can still start default session (with empty session name) with just:

::

    copyq

In the same manner you can manipulate the session. E.g. to add an item
to first tab in ``test1`` session, run:

::

    copyq --session=test1 add "Some text"

Default session has empty name but it can be overridden by setting
``COPYQ_SESSION_NAME`` environment variable.

You need to use same session name for clients launched outside the application.

::

    $ copyq -s test2 tab
    ERROR: Cannot connect to server! Start CopyQ server first.

    $ copyq -s test1 tab
    &clipboard

Configuration Path
------------------

Current configuration path can be overridden with ``COPYQ_SETTINGS_PATH``
environment variable.

::

    $ copyq info config
    /home/user/.config/copyq/copyq.conf

    $ COPYQ_SETTINGS_PATH=$HOME/copyq-settings copyq info config
    /home/user/copyq-settings/copyq/copyq.conf

You need to use same configuration path (and session name) for clients launched
outside the application.

::

    $ copyq tab
    ERROR: Cannot connect to server! Start CopyQ server first.

    $ COPYQ_SETTINGS_PATH=$HOME/copyq-settings copyq tab
    &clipboard

Item Data Path
--------------

.. versionchanged:: 17.0.0
   Tab data files (``copyq_tab_*.dat``) moved from the configuration
   directory to the data directory. Existing files are migrated
   automatically on first startup.

Tab data files and item data are stored in the data directory.
The path can be overridden with ``COPYQ_ITEM_DATA_PATH``.

::

    $ copyq info data
    /home/user/.local/share/copyq/items

Item data that exceeds a size threshold is stored in separate files
(in a directory structure based on data checksum) and only referenced
from the tab data files. The default threshold can be overridden with
``item_data_threshold`` option. Setting it to a negative value disables
the separate storage and keeps everything in the tab data files.

State Path
----------

.. versionchanged:: 17.0.0
   State files were previously stored in the configuration directory.
   They are migrated automatically on first startup.

UI state files (window geometry, collapsed tabs, filter history, action
dialog history) and logs are stored in the state directory. On Linux this
follows ``$XDG_STATE_HOME`` (default ``~/.local/state/copyq``). On Windows
and macOS state files are stored alongside data.

The path can be overridden with ``COPYQ_STATE_PATH``.
The log directory can be overridden separately with ``COPYQ_LOG_DIR``.

::

    $ copyq info state
    /home/user/.local/state/copyq

    $ copyq info log
    /home/user/.local/state/copyq/logs/copyq-20250101-12345.log

Existing state files and logs are migrated automatically from the
previous locations on first startup of version 17.0.0 or later.

Icon Color
----------

Icon for each session is bit different. The color is generated from session
name and can be changed using ``COPYQ_SESSION_COLOR`` environment variable.

::

    COPYQ_SESSION_COLOR="yellow" copyq
    COPYQ_SESSION_COLOR="#f90" copyq

On Linux, changing icon color won't work if current icon theme contains icon
named "copyq-normal" or doesn't contain "copyq-mask".
Use ``COPYQ_DEFAULT_ICON`` environment variable to avoid using the application
icon from icon theme.

::

    COPYQ_DEFAULT_ICON=1 copyq

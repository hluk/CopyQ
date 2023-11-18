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

Item data path can be overridden with ``COPYQ_ITEM_DATA_PATH``
environment variable.

::

    $ copyq info data
    /home/user/.local/share/copyq/copyq

    $ COPYQ_ITEM_DATA_PATH=$HOME/copyq-data copyq info data
    /home/user/copyq-data/copyq/copyq.conf

The directory contains data for items that exceeds 4096 bytes. The default
threshold can be overridden with ``item_data_threshold`` option.

::

    $ copyq config item_data_threshold
    4096

To disable using the data directory and store everything into tab data files,
set the threshold to a negative value. The tab data file will be updated only
after the items in the tab change.

::

    $ copyq config item_data_threshold -1
    -1

Note: Using data directory ensure that the application is fast even if there
are a lot of large items in tabs.

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

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

Current configuration path can be overriden with ``COPYQ_SETTINGS_PATH``
environment variable.

::

    $ copyq info config
    /home/user/.config/copyq/copyq.conf

    $ COPYQ_SETTINGS_PATH=$HOME/copyq-settings copyq info config
    /home/user/copyq-settings/copyq/copyq.conf

You need to use same configuration path (and session name) for clients lauched
outside the application.

::

    $ copyq tab
    ERROR: Cannot connect to server! Start CopyQ server first.

    $ COPYQ_SETTINGS_PATH=$HOME/copyq-settings copyq tab
    &clipboard

Icon Color
----------

Icon for each session is bit different. The color is generated from session
name and can be changed using ``COPYQ_SESSION_COLOR`` environment variable.

::

    COPYQ_SESSION_COLOR="yellow" copyq
    COPYQ_SESSION_COLOR="#f90" copyq

.. note::

    On Linux, changing icon color won't work if current icon theme contains
    icon named "copyq-normal" or doesn't contain "copyq-mask" (and
    "copyq-busy-mask").

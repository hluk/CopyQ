.. _sessions:

Sessions
========

You can run multiple instances of the application given that they have
different session names.

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

Icon for each session is bit different. The color is generated from session
name and can be changed using ``COPYQ_SESSION_COLOR`` environment variable.

::

    COPYQ_SESSION_COLOR="yellow" copyq
    COPYQ_SESSION_COLOR="#f90" copyq

.. note::

    On Linux, changing icon color won't work if current icon theme contains
    icon named "copyq-normal" or doesn't contain "copyq-mask" (and
    "copyq-busy-mask").

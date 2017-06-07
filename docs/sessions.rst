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

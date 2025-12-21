Security
========

This page describes how CopyQ handles the clipboard data and how to make the
clipboard safer.

Data Storage
============

By default, Any text or image in the clipboard is stored automatically in CopyQ.

You can completely disable automatic clipboard storing or avoid storing content
copied from windows with matching window titles.

.. seealso::

    - :ref:`faq-disable-clipboard-storing`
    - :ref:`faq-ignore-password-manager`

The data from all tabs are stored in the configuration directory unencrypted
(unless the Encryption is enabled).

.. seealso::

    - :ref:`faq-config-path`
    - :ref:`encrypt`

CopyQ does not collect any other data and does not send anything over network.

Clipboard Content Visibility
============================

The clipboard content is normally shown in the item list of a clipboard tab,
main window title, tray tool tip and notification.

To disable showing the current clipboard in GUI, use `"No Clipboard
in Title and Tool Tip" command
<https://github.com/hluk/copyq-commands/tree/master/Scripts#no-clipboard-in-title-and-tool-tip>`__
and disable notifications (in Preferences).

.. seealso::

    - :ref:`faq-disable-notifications`
    - :ref:`faq-share-commands`

Clipboard Access
================

Usually other applications can access clipboard content without any
restriction.

To restrict accessing the data after a long time, use `"Clear Clipboard After
Interval" command
<https://github.com/hluk/copyq-commands/tree/master/Scripts#clear-clipboard-after-interval>`__.

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

.. _secret-clipboard:

Secret Clipboard Data
=====================

Password managers, web browsers in private or incognito mode, remote desktop
software (such as RDP or Citrix), and virtualization tools (such as VMware or
VirtualBox) often mark clipboard content as secret. CopyQ recognizes this
and **silently ignores the clipboard change** — by default, the data is not
stored, not shown, and not processed by automatic commands.

Detection is platform-specific. The clipboard formats that indicate secrets are:

- **Linux** — ``x-kde-passwordManagerHint`` set to ``secret``. Used by
  KeePassXC, KDE Wallet, and other password managers.
- **macOS** — Presence of ``application/x-nspasteboard-concealed-type``. Used by
  Keychain Access, 1Password, and other macOS applications.
- **Windows** — Any of the following formats:

  - ``Clipboard Viewer Ignore``
  - ``ExcludeClipboardContentFromMonitorProcessing``
  - ``CanIncludeInClipboardHistory`` set to ``0``
  - ``CanUploadToCloudClipboard`` set to ``0``

  These are `documented by Microsoft
  <https://learn.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats#cloud-clipboard-and-clipboard-history-formats>`__
  and used by Windows Credential Manager and most third-party password managers.

In all cases, CopyQ adds the ``application/x-copyq-secret`` format (available in
scripts as :js:data:`mimeSecret`) and hands the data to the
``onSecretClipboardChanged()`` callback. The default implementation discards
everything except the secret marker, so no sensitive content reaches CopyQ's
history or automatic commands.

To check whether the current clipboard content is considered secret, open
**File — Show Clipboard Content** (default shortcut ``Ctrl+Shift+C``) in the
main window. If any of the formats listed above appear in the clipboard data,
CopyQ treats the content as secret.

Overriding Secret Detection
----------------------------

If CopyQ is ignoring clipboard data that you want to capture — for example,
content from a private browser window or a remote desktop session — you can
override the default behavior.

The safer approach is to allow secret clipboard data only from specific windows.
The following Script command processes secrets only when the source window title
contains "Chromium", "Chrome", or "Firefox" (adjust the pattern to match your
browser or application):

.. code-block:: ini

    [Command]
    Command="
        const onSecretClipboardChanged_ = onSecretClipboardChanged
        onSecretClipboardChanged = function() {
            const title = str(data(mimeWindowTitle))
            if (title.match(/Chromium|Chrome|Firefox/i))
                onClipboardChanged()
            else
                onSecretClipboardChanged_()
        }
        "
    IsScript=true
    Name=Store Secrets from Browser
    Icon=\xf21b

This keeps passwords from password managers safely ignored while allowing
clipboard data from private browsing windows.

Alternatively, to process **all** secret clipboard data regardless of source:

.. code-block:: ini

    [Command]
    Command="global.onSecretClipboardChanged = global.onClipboardChanged"
    IsScript=true
    Name=Store/Process All Secrets
    Icon=\xf09c

.. warning::

    Processing all secrets means **passwords from password managers will be
    stored in CopyQ's unencrypted history**. If you enable this, consider also
    enabling encryption (see :ref:`encrypt`) to protect stored items, or use
    selective automatic commands to filter what gets saved.

.. seealso::

    - :js:func:`onSecretClipboardChanged` — scripting API reference
    - :ref:`faq-ignore-password-manager` — ignoring password manager windows by title
    - :ref:`encrypt` — encrypting stored clipboard data


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

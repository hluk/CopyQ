Known Issues
============

This document lists known commonly occurring issues and possible solutions.

.. _known-issue-window-tray-hidden:

On Windows, tray icon is hidden/repositioned after restart
----------------------------------------------------------

With current official builds of CopyQ, the tray icon position and hide/show
status are not restored after the application is restarted or after logging in.

**Workaround** is to use CopyQ binaries build with older Qt framework version (Qt
5.9); these are provided in latest comments in the issue link below.

.. seealso::

    `Issue #1258 <https://github.com/hluk/CopyQ/issues/1258>`__

.. _known-issue-windows-console-output:

On Windows, CopyQ does not print anything on console
----------------------------------------------------

On Windows, you may not see any output when executing CopyQ in a
console/terminal application (PowerShell or cmd).

**Workarounds:**

* Use different console application: Git Bash, Cygwin or similar.
* Use Action dialog in CopyQ (``F5`` shortcut) and set "Store standard output"
  to "text/plain" to save the output as new item in current tab.
* Append ``| Write-Output`` to commands in PowerShell:

  .. code-block:: powershell

    & 'C:\Program Files\CopyQ\copyq.exe' help | Write-Output

.. seealso::

    `Issue #349 <https://github.com/hluk/CopyQ/issues/349>`__

.. _known-issue-macos-paste-after-install:

On macOS, CopyQ won't paste after installation/update
-----------------------------------------------------

CopyQ is not signed app, you need to grant Accessibility again when it's
installed or updated.

**To fix this**, try following steps:

1. Go to System Preferences -> Security & Privacy -> Privacy -> Accessibility
   (or just search for "Allow apps to use Accessibility").
2. Click the unlock button.
3. Select CopyQ from the list and remove it (with the "-" button).

.. seealso::

    - `Issue #1030 <https://github.com/hluk/CopyQ/issues/1030>`__
    - `Issue #1245 <https://github.com/hluk/CopyQ/issues/1245>`__

.. _known-issue-wayland:

On Linux, global shortcuts, pasting or clipboard monitoring does not work
-------------------------------------------------------------------------

This can be caused by running CopyQ under a **Wayland** window manager instead
of the X11 server.

Depending on the desktop environment, these features may not be supported:

- global shortcuts
- clipboard monitoring
- pasting from CopyQ and issuing copy command to other apps (that is passing
  shortcuts to application)
- screenshot functionality
- querying keyboard modifiers and mouse position

**Workaround** for most problems is to set ``QT_QPA_PLATFORM`` environment variable
and use Xwayland (e.g. ``xorg-x11-server-Xwayland`` Fedora package).

For example, launch CopyQ with::

    env QT_QPA_PLATFORM=xcb copyq

If CopyQ autostarts, you can change ``Exec=...`` line in
``~/.config/autostart/copyq.desktop``::

    Exec=env QT_QPA_PLATFORM=xcb copyq

.. note::

    Mouse selection will still work only if the source application itself
    supports it.

.. seealso::

    `Wayland Support
    <https://github.com/hluk/copyq-commands/tree/master/Scripts#wayland-support>`__
    command reimplements some features on Wayland through external tools (see
    `README <https://github.com/hluk/copyq-commands/blob/master/README.md>`__
    for details on how to add the command.

    `Issue #27 <https://github.com/hluk/CopyQ/issues/27>`__

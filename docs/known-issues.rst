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

.. _known-issue-gnome:

On GNOME, new clipboard is not stored
-------------------------------------

The app requires the CopyQ Clipboard Monitor GNOME Shell extension to be
enabled so it can watch clipboard changes and store them. The extension is
shipped with CopyQ 14.0.0 and later.

.. note::

    The GNOME extension is only available when CopyQ is installed on the system
    (e.g. from a package manager). It will **not** work when running CopyQ as a
    Flatpak or AppImage because the extension cannot be registered with the
    GNOME Shell from a sandboxed environment.

.. seealso::

    :ref:`known-issue-wayland`

.. _known-issue-wayland:

On Linux, some features do not work under Wayland
--------------------------------------------------

When running CopyQ under a **Wayland** compositor, some features may not work
depending on the desktop environment and the protocols it supports.

**Global shortcuts** work natively if the desktop environment provides
Portal support (``xdg-desktop-portal``).

**Clipboard monitoring** works natively if the compositor supports the required
Wayland protocol. This works on KDE Plasma, Sway, Hyprland and other
wlroots-based compositors. On **GNOME**, this protocol is not supported, but
CopyQ ships a GNOME Shell extension that provides clipboard monitoring instead
(see :ref:`known-issue-gnome`). If clipboard monitoring does not work with
either method, see :ref:`wayland-xwayland-fallback` below.

Querying **state of keyboard modifiers** (for example pressed Shift, Ctrl etc.
which can be useful for some custom commands) and **mouse position** (useful
for positioning menus) **does not work** on Wayland.

See the subsections below for other fixes.

Workaround: Wayland Support command
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This can fix (depending on the desktop environment and installed tools):

- pasting from CopyQ and issuing copy commands to other apps
- screenshot functionality
- retrieving and matching window titles

Install the `Wayland Support
<https://github.com/hluk/copyq-commands/tree/master#wayland-support>`__
command to fix the features. It also requires some external tools to be
installed on the system.

.. _wayland-xwayland-fallback:

Workaround: running under XWayland
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This can fix:

- clipboard monitoring
- setting and restoring window position (only window size is supported by most
  Wayland compositors natively)

Setting ``QT_QPA_PLATFORM=xcb`` environment variable forces CopyQ to run under
XWayland mode. Unfortunately, it can cause clipboard monitoring to fail when
the main window is closed, X11 connection errors, and other issues depending on
the XWayland implementation.

To start CopyQ under XWayland, use:

.. code-block:: bash

    env QT_QPA_PLATFORM=xcb copyq

If CopyQ autostarts, you can change the ``Exec=...`` line in
``~/.config/autostart/copyq.desktop``::

    Exec=env QT_QPA_PLATFORM=xcb copyq

For the **Flatpak** application, see `this workaround
<https://github.com/hluk/CopyQ/issues/2948#issuecomment-2614271330>`__.

.. note::

    Mouse selection will still work only if the source application itself
    supports it.

.. seealso::

    `Issue #27 <https://github.com/hluk/CopyQ/issues/27>`__

    `Issue #3587 <https://github.com/hluk/CopyQ/issues/3587>`__
    — ``QT_QPA_PLATFORM=xcb`` can break clipboard monitoring


.. _known-issue-gnome-busy-cursor:

On GNOME, busy cursor after selecting tray menu item
----------------------------------------------------

When using the AppIndicator/KStatusNotifierItem GNOME Shell extension, selecting
an item from the CopyQ tray menu may cause a busy cursor for about 15 seconds.
This is a bug in the extension — it triggers startup notification unconditionally
without checking the application's ``StartupNotify`` desktop entry key.

.. seealso::

    - `Issue #3586 <https://github.com/hluk/CopyQ/issues/3586>`__
    - `AppIndicator extension issue #443
      <https://github.com/ubuntu/gnome-shell-extension-appindicator/issues/443>`__
Scripting command "copy()" fails
--------------------------------

The command ``copy()`` sends the Ctrl+C shortcut to the current window.
This can fail depending on the active application.
If CopyQ won't detect a clipboard change, it throws an exception.
The execution then fails with the message ``Failed to copy to clipboard!``.

An alternative under Windows is to use a Powershell script to override the ``copy`` operation
(see :ref:`faq-share-commands`):

.. code-block:: powershell

    [Command]
    Command="
        copy = function() {
            execute('powershell', '-Command', `
                Add-Type -AssemblyName System.Windows.Forms;
                Start-Sleep -Milliseconds 300;
                [System.Windows.Forms.SendKeys]::SendWait(\"^c\");
                Start-Sleep -Milliseconds 300;
            `);
        }"
    IsScript=true
    Name=Override copy()

The delays are added to make sure no focus issues occur and the text is copied to the clipboard.


.. _known-issue-focus-stealing:

CopyQ steals keyboard focus when showing window or menu
-------------------------------------------------------

When CopyQ shows its main window or tray menu (via global shortcut, tray icon
click, or script), it transfers keyboard focus from the previously active
window.  This can cause side effects in the target application such as:

* File renames aborting (e.g. pressing F2 in file managers)
* Combo boxes selecting all their content
* Short-lived widgets dismissing (Start menu, PowerToys, extension dialogs)
* Application menus closing
* Auto-hide windows hiding (e.g. ConEmu Quake mode)

.. seealso::
    - `Issue #3540 <https://github.com/hluk/CopyQ/issues/3540>`__

Workaround
^^^^^^^^^^
Use global shortcuts to cycle through clipboard history and
paste without showing the CopyQ window.

The `Cycle Items - Quick
<https://github.com/hluk/copyq-commands/blob/master/README.md#cycle-items---quick>`__
command previews items in notifications and automatically pastes the selected item
when the shortcut modifier(s) are released―without ever opening the main
window.

A simpler alternative is to assign a global shortcut to ``next()`` or
``previous()``. These functions cycle through clipboard history and update the system
clipboard silently. For example, the following command updates to the next item:

.. code-block:: js

    next()

The item can then be pasted with the normal system paste shortcut
(``Ctrl+V`` or ``Cmd+V``).

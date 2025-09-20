Pin Items
=========

This page describes how to pin selected items in a tab so they cannot be
accidentally removed or moved from current row.

Why pin items?
--------------

There are two main reasons to pin items.

If a new item is added to a list (e.g. automatically when clipboard changed),
rest of the items need to move one row down, except pinned items which stay on
the same row. This is useful to **pin important items to the top of the list**.

If a tab is full (see option "Maximum number of items in history" in "History"
configuration tab), adding a new item removes old item from bottom of the list.
**Pinned items cannot be removed** so the last unpinned item is removed
instead.

.. note::

    New items cannot be added to a tab if all its items are pinned and the tab
    is full.

Configuration
-------------

.. note::

    On Windows, to enable this feature you need to install "Pinned Items"
    plugin.

To enable this functionality, assign keyboard shortcut for Pin and Unpin
actions in "Shortcuts" tab in Preferences (``Ctrl+P``).

.. note::

    Keyboard shortcut for both menu items can be the same since at most one of
    the menu items is always visible.

Pinning Items
-------------

If set up correctly, when you select items, Pin action should be available in
toolbar, context menu and "Item" menu.

Selecting the Pin menu item (or pressing assigned keyboard shortcut) will pin
selected items to their current rows.

Pinned items will show with **gray bar on the right side** in the list.

Deleting pinned items won't work, unpin the items first.
Unpin action is available if an pinned item is selected.

Pinned items also will stay in same rows unless you **move them with mouse or
using keyboard shortcuts** (``Ctrl+Up/Down/Home/End``).

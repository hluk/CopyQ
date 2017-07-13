Synchronize with Documents
==========================

This page describes how to keep items in a tab synchronized with files in a
directory on a disk (or a Dropbox folder).

Configuration
-------------

.. note::

    On Windows, to enable this feature you need to install "Synchronize"
    plugin.

Set path synchronization directory for a tab.

1. Open "Preferences" (``Ctrl+P`` shortcut),
2. go to "Items" tab,
3. select "Synchronize",

    .. image:: images/synchronize-config.png
       :scale: 50%
       :alt: Configure synchronization directory

4. double-click an empty space in **Tab Name** column and enter name of the tab to synchronize,
5. click the browse button on the same row and select directory for the tab,
6. click OK to save changes.

Now any items in the synchronized tab will be saved in the directory and
existing files will show up in the tab even if the tab or a file is created
later.

Synchronized items can be copied and edited as normal items.

File Types
----------

Only files with known format can be shown as items. By default
files with ``.txt`` suffix show up as text items,
files with ``.html`` suffix show up as formatted text items,
files with ``.png`` suffix show up as images etc.

To show other files as items you need to set their file suffix in the second
table in the configuration. Here you can set icon and MIME format for the file
data.

    .. image:: images/synchronize-formats.png
       :scale: 50%
       :alt: Configure file formats to show

The configuration in the image above allows for content of all files with
``.cpp`` suffix in synchronized directories to show up text items, i.e. items
have ``text/plain`` format containing the file data.


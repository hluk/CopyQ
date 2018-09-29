Tabs and Items
==============

.. _tabs:

Tabs
----

Tabs are means to organize texts, images and other data.

Initially there is only one tab which is used for storing clipboard and
the tab bar is hidden.

User can create new tabs from "Tabs" menu or using ``Ctrl+T``. The tab
bar will appear if there is more than one tab. Using mouse, user can
reorder tabs and drop items and other data into tabs.

If tab name contains ``&``, the following letter is used for quick
access to the tab (the letter is underlined in tab bar or tab tree and
``&`` is hidden). For example, tab named "&Clipboard" can be opened
using ``Alt+C`` shortcut.

Option "Tab Tree" enables user to organize tabs into groups. Tabs with
names "Job/Tasks/1" and "Job/Tasks/2" will create following structure in
tab tree.

::

    > Job
        > Tasks
            > 1
            > 2

Storing Clipboard
-----------------

If "Store Clipboard" option is enabled (under "General" tab in config
dialog) and "Tab for storing clipboard" is set (under "History" tab in
config dialog), every time user copies something to clipboard a new item
will be created in that particular tab. The item will contain only text
and data that are needed by plugins (e.g. plugin "Images" requires
``image/svg``, ``image/png`` or similar).

Organizing Items
----------------

Any data or item can be moved or copied to other tab by dragging it
using mouse or by pasting it in item list.

Commands can automatically organize items into tabs. For example,
following command will put copied images to "Images" tab (to use the
command, copy it to the command list in configuration).

.. code-block:: ini

    [Command]
    Name=Move Images to Other Tab
    Input=image/png
    Automatic=true
    Remove=true
    Icon=\xf03e
    Tab=&Images

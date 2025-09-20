.. _commands-display:

Display Commands
================

Display command is type of command that modifies item data before displaying.
The modified data are only used for displaying the item and are not stored.

The command is executed just before an item needs to be displayed. This can
sometimes happen multiple times for the same item if the data or
configuration changes or the tab was unloaded.

Display commands can be created in Command dialog by setting Type of Action
to :ref:`command-dialog-display`.

Use ``data()`` to retrieve current item data and ``setData()`` to set the
data to display (these are not stored permanently).

E.g. use slightly different color for plain text items.

.. code-block:: js

    copyq:
    if ( str(data(mimeText)) && !str(data(mimeHtml)) ) {
        html = escapeHtml(data(mimeText))
        setData(mimeHtml, '<span style="color:#764">' + html + '</span>')
    }

E.g. try to interpret text as Markdown (with ``marked`` external utility).

.. code-block:: js

    copyq:
    var text = data(mimeText)
    var result = execute('marked', null, text)
    if (result && result.exit_code == 0) {
        setData(mimeHtml, result.stdout)
    }

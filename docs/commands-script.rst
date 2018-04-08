.. _commands-script:

Script Commands
===============

Script command is type of command which allows overriding existing functions
and creating new ones (allowing new command line arguments to be used).

The command is executed before any script and all defined variables and
functions are available to the scripts.

Script commands can be created in Command dialog by setting Type of Action to
:ref:`command-dialog-script`.

Extending Command Line Interface
--------------------------------

By adding following script command you can use ``hello()`` from other script
or on command line (``copyq hello``).

.. code-block:: js

    function hello() {
        print('Hello, World!\n')
    }

It's useful to move code used by multiple commands to a new script command.

It can also simplify using ``copyq`` from another application or shell script.

Override Functionality
----------------------

Existing functions can be overridden from script commands.

Specifically :any:`onClipboardChanged` and functions it calls can be
overridden to customize handling of new clipboard content.

E.g. following command saves only textual clipboard data and removes any
formatted text.

.. code-block:: js

    saveData_ = saveData

    saveData = function() {
        if ( str(data(mimeText)) != "" ) {
            popup('Saving only text')
            removeData(mimeHtml)
            saveData_()
        } else {
            popup('Not saving non-textual data')
        }
    }

E.g. following command overrides ``paste()`` to use an external utility for
pasting clipboard.

.. code-block:: js

    paste = function() {
        var x = execute(
            'xdotool',
            'keyup', 'alt', 'ctrl', 'shift', 'super', 'meta',
            'key', 'shift+Insert')
        if (!x)
            throw 'Failed to run xdotool'
        if (x.stderr)
            throw 'Failed to run xdotool: ' + str(x.stderr)
    }

E.g. show custom notifications for clipboard and X11 selection changes.

.. code-block:: js

    function clipboardNotification(owns, hidden) {
        var id = isClipboard() ? 'clipboard' : 'selection'
        var icon = isClipboard() ? '\uf0ea' : '\uf246'
        var owner = owns ? 'CopyQ' : str(data(mimeWindowTitle))
        var title = id + ' - ' + owner
        var message = hidden ? '<HIDDEN>' : data(mimeText).left(100)
        notification(
        '.id', id,
        '.title', title,
        '.message', message,
        '.icon', icon
        )
    }

    onClipboardChanged_ = onClipboardChanged
    onClipboardChanged = function() {
        clipboardNotification(false, false)
        onClipboardChanged_()
    }

    onOwnClipboardChanged_ = onOwnClipboardChanged
    onOwnClipboardChanged = function() {
        clipboardNotification(true, false)
        onOwnClipboardChanged_()
    }

    onHiddenClipboardChanged_ = onHiddenClipboardChanged
    onHiddenClipboardChanged = function() {
        clipboardNotification(true, true)
        onHiddenClipboardChanged_()
    }
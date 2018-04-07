.. _commands-script:

Script Commands
===============

Script commands can be created in Command dialog by setting Type of Action to
:ref:`command-dialog-script`.

The command is script which is loaded before any other script is started.
This allows overriding existing functions and creating new ones (allowing new
command line arguments to be used).

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
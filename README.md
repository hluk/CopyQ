CopyQ
=====

[![Build Status](https://travis-ci.org/hluk/CopyQ.png)](https://travis-ci.org/hluk/CopyQ)

CopyQ is clipboard manager with searchable and editable history.

To build and install CopyQ on your system read `INSTALL` file.

To start CopyQ run `copyq` command without parameters. The application main
window is accessible by clicking on system tray icon or running `copyq toggle`.

To exit the application middle-click on the tray icon or press Ctrl-Q keys
in the application window.

For more info go to <http://code.google.com/p/copyq/>.

Dependencies
------------

To compile and run the application you'll need the latest stable version of
[Qt](http://qt.digia.com/) library (there is also experimental support for
[Qt 5](http://qt-project.org/wiki/Qt_5.0)).

Optional dependency is [QtWebKit](http://trac.webkit.org/wiki/QtWebKit) which
enables the application to use advanced HTML rendering and fetching remote
images and other data.

Additionally X11 requires XFixes extension to be installed (fixes some
clipboard issues).

Keyboard navigation
-------------------
* `PgDown/PgUp`, `Home/End`, `Up/Down`

    item list navigation

* `Left`, `Right`, `Tab`, `Shift+Tab`

    tab navigation

* `Ctrl+Up`, `Ctrl+Down`

    move selected items

* `Ctrl+Left`, `Ctrl+Right`

    cycle through item formats

* `Escape`

    hide window

* `Ctrl+Q`

    exit

* `F2`

    edit selected items (in place)

* `Ctrl+E`

    edit items in external editor

* `F5`

    open action dialog for selected items

* `Delete`

    delete selected items

* `Enter`

    put current item into clipboard

* `F1`

    show help

Type any number to select item with given id.

Type a regular expressions (case-insensitive) to search/filter items.

Command Line Interface
----------------------

    Usage: copyq [COMMAND]

    Starts server if no command is specified.
      COMMANDs:
        show                     Show main window.
        hide                     Hide main window.
        toggle                   Show or hide main window.
        menu                     Open context menu.
        exit                     Exit server.

        clipboard [MIME]         Print clipboard content.
        selection [MIME]         Print X11 selection content.

        length, count, size      Print number of items in history.
        select [ROW=0]           Move item in the row to clipboard.
        add TEXT...              Add text into clipboard.
        insert ROW TEXT          Insert text into given row.
        remove [ROWS=0...]       Remove items in given rows.
        edit [ROWS...]           Edit items or edit new one.
                                 Value -1 is for current text in clipboard.

        read [MIME|ROW]...       Print raw data of clipboard or item in row.
        write [ROW=0] MIME DATA  Write raw data to given row.
        separator SEPARATOR      Set separator for items on output.

        action [ROWS=0...]       Show action dialog.
        action [ROWS=0...] [PROGRAM [SEPARATOR=\n]]
          Run PROGRAM on item text in the rows.
          Use %1 in PROGRAM to pass text as argument.
        popup TITLE MESSAGE [TIME=8000]
          Show tray popup message for TIME milliseconds.

        tab                      List available tab names.
        tab NAME [COMMAND]       Run command on tab with given name.
                                 Tab is created if it doesn't exist.
                                 Default is the first tab.
        removetab NAME           Remove tab.
        renametab NAME NEW_NAME  Rename tab.

        exporttab FILE_NAME      Export items to file.
        importtab FILE_NAME      Import items from file.

        config                   List all options.
        config OPTION            Get option value.
        config OPTION VALUE      Set option value.

        eval, -e [SCRIPT]        Evaluate ECMAScript program.
        help, -h, --help [COMMAND]
          Print help for COMMAND or all commands.
        version, -v, --version
          Print version of program and libraries.

    NOTES:
      - Changing first item (ROW is 0) will also change clipboard.
      - Use dash argument (-) to read data from stdandard input.
      - Use double-dash argument (--) to read all following arguments without
        expanding escape sequences (i.e. \n, \t and others).
      - Use ? for MIME to print available MIME types (default is "text/plain").

Usage Examples
--------------
CopyQ must be running to be able to issue commands using command line.
Most of the examples should work on GNU/Linux with the correct applications
installed.

To start CopyQ run following command:

    copyq

Insert text to the clipboard:

    copyq add "print([x**2 for x in range(10)])"

and process it in Python interpreter:

    copyq action python

The result will be copied to the clipboard.

For each file in given directory create new item:

    copyq action "ls /"

Load file content into clipboard:

    copyq action "cat file.txt" ""

Note: Last argument is separator - empty string means "create single item".

Process an item with the Python interpreter and redirect the standard output
to the standard error output using sh command (shell):

    copyq add 'print("Hello world!")'
    copyq action 'sh -c "python 1>&2"'
    copyq read 0

Note: Standard error output will be show as tray icon tooltip.

To concatenate items select them items in CopyQ window and press F5 key,
type `cat` into command input field, check `Output into item(s)` check box,
clear `Separator field` and press Ok button to submit.

Monitor file (or pipe) `$HOME/clipboard` and load every new line into clipboard:

    copyq action "tail -f $HOME/clipboard"

This process can be killed by right clicking on tray icon and selecting
the process from context menu.

Find files in current directory:

    copyq action "find \"$PWD\" -iname '*.cpp'"

Open CopyQ window and select one of the found files from history. Open action
dialog (press F5 key) and in the command field type your favorite text editor
(e.g. `gedit %1`; `%1` will be replaced with temporary filename containing
selected text).

To copy an image to clipboard use for example:

    copyq write image/gif - < image.gif
    copyq write image/svg - < image.svg


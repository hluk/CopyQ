CopyQ
=====
CopyQ is clipboard manager with searchable and editable history.

To build and install CopyQ on your system read `INSTALL` file.

To start CopyQ run `copyq` command without parameters. The application main
window is accessible by clicking on system tray icon or running `copyq toggle`.

To exit the application middle-click on the tray icon or press Ctrl-Q keys
in the application window.

Keyboard navigation
-------------------
* `PgDown/PgUp`, `Home/End`, `Up/Down`

    item list navigation

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

    usage: copyq [COMMAND]
    Starts server if no command is specified.
      COMMANDs:
        show    show main window
        hide    hide main window
        toggle  show or hide main window
        menu    open context menu
        exit    exit server

        clipboard [mime_type="text/plain"]
          print clipboard content
        selection [mime_type="text/plain"]
          print X11 selection content

        length, count, size
          print number of items in history
        select [row=0]
          move item in the row to clipboard
        add <text> ...
          add text into clipboard
        remove [row=0] ...
          remove item in given rows
        edit [row=0] ...
          edit clipboard item

        read [mime_type="text/plain"|row] ...
          print raw data of clipboard or item in row
          use ? for mime_type to print mime types
        write mime_type data ...
          write raw data to clipboard
        - [mime_type="text/plain"]
          copy text from standard input into clipboard

        action [row=0] ...
          show action dialog
        action [row=0] ... "command" [separator=\n]
          apply command on item text in the row

        help, -h, --help
          print this help

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

    copyq - image/gif < image.gif
    copyq - image/svg < image.svg


CopyQ
=====

[![Translation Status](https://hosted.weblate.org/widgets/copyq/-/svg-badge.svg)](https://hosted.weblate.org/engage/copyq/?utm_source=widget)
[![Build Status](https://travis-ci.org/hluk/CopyQ.svg?branch=master)](https://travis-ci.org/hluk/CopyQ)
[![Windows Build Status](https://ci.appveyor.com/api/projects/status/github/hluk/copyq?branch=master&svg=true)](https://ci.appveyor.com/project/hluk/copyq)
[![Coverage Status](https://coveralls.io/repos/hluk/CopyQ/badge.svg?branch=master)](https://coveralls.io/r/hluk/CopyQ?branch=master)

CopyQ is advanced clipboard manager with editing and scripting features.
- [Downloads](https://github.com/hluk/CopyQ/releases)
- [Web Site](https://hluk.github.io/CopyQ/)
- [Wiki](https://github.com/hluk/CopyQ/wiki)
- [Mailing List](https://groups.google.com/group/copyq)
- [Bug Reports](https://github.com/hluk/CopyQ/issues)
- [Donate](https://www.bountysource.com/teams/copyq)
- [Scripting Reference](https://github.com/hluk/CopyQ/blob/master/src/scriptable/README.md)

Overview
--------

CopyQ monitors system clipboard and saves its content in customized tabs.
Saved clipboard can be later copied and pasted directly into any application.

Items can be:

* edited with internal editor or with preferred text editor,
* moved to other tabs,
* drag'n'dropped to applications,
* marked with tag or a note,
* passed to or changed by custom commands,
* or simply removed.

Features
--------

* Support for Linux, Windows and OS X 10.9+
* Store text, HTML, images or any other custom formats
* Quickly browse and filter items in clipboard history
* Sort, create, edit, remove, copy/paste, drag'n'drop items in tabs
* Add notes or tags to items
* System-wide shortcuts with customizable commands
* Paste items with shortcut or from tray or main window
* Fully customizable appearance
* Advanced command-line interface and scripting
* Ignore clipboard copied from some windows or containing some text
* Support for simple Vim-like editor and shortcuts
* Many more features

Install and Run
---------------

To install CopyQ, use binary package or installer provided for your system or
follow instructions in
[INSTALL](https://github.com/hluk/CopyQ/blob/master/INSTALL) to build the
application.

To start CopyQ run `copyq` command without parameters. The application main
window is accessible by clicking on system tray icon or running `copyq toggle`.

To exit the application select Exit from tray menu or press Ctrl-Q keys in the
application window.

Developers and Translators
-------------------------

If you want to help with translating, fixing or writing code read
[HACKING](https://github.com/hluk/CopyQ/blob/master/HACKING) file.

Dependencies
------------

To build and run the application you'll need [Qt](https://www.qt.io/download/)
library. To compile on OS X, you will need at least Qt 5.2.

Optional dependency is [QtWebKit](https://trac.webkit.org/wiki/QtWebKit) which
enables the application to use advanced HTML rendering and fetching remote
images and other data. This is available through ItemWeb plugin.

Additionally X11 requires XFixes extension to be installed (fixes some
clipboard issues).

Optional dependency for X11 is XTest extension (Ubuntu package `libxtst6` and
`libxtst-dev` for compilation). This is needed for some applications like
`gedit` so that automatic pasting works correctly.

Keyboard navigation
-------------------
* `PgDown/PgUp`, `Home/End`, `Up/Down`

    item list navigation

* `Left`, `Right`, `Ctrl+Tab`, `Ctrl+Shift+Tab`

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

Type a regular expressions (case-insensitive) to search/filter items.

On OS X, use Command instead of Ctrl for the shortcuts above.

Usage Examples
--------------
CopyQ must be running to be able to issue commands using command line.
Most of the examples should work on GNU/Linux with the correct applications
installed.

To start CopyQ run following command:

    copyq

To print useful command line arguments or help for particular arguments:

    copyq --help
    copyq --help add
    copyq --help tab

Insert text to the clipboard:

    copyq add "print([x**2 for x in range(10)])"

and process it in Python interpreter:

    copyq action python

The result can be copied to the clipboard with:

    copyq select 0

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
clear `Separator field` and press OK button to submit.

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


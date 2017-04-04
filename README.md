# CopyQ

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

## Overview

CopyQ monitors system clipboard and saves its content in customized tabs.
Saved clipboard can be later copied and pasted directly into any application.

Items can be:

* edited with internal editor or with preferred text editor,
* moved to other tabs,
* drag'n'dropped to applications,
* marked with tag or a note,
* passed to or changed by custom commands,
* or simply removed.

## Features

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

## Install and Run

To install CopyQ, use the binary package or installer provided for your system. For system-specific information, please see below. For unlisted systems, please follow the instructions in
[INSTALL](https://github.com/hluk/CopyQ/blob/master/INSTALL) to build the
application.

### Windows

On Windows you can install [Chocolatey package](https://chocolatey.org/packages/copyq).

### Ubuntu

Install and keep CopyQ always up to date by running the following three commands from the terminal:

```bash
$ sudo add-apt-repository ppa:hluk/copyq
$ sudo apt update
$ sudo apt install copyq
```

### OS X

On OS X you can use [Homebrew](https://brew.sh/) to install the app.

```bash
brew cask install copyq
```

### Starting CopyQ

To start CopyQ run `copyq` command without parameters. The application main
window is accessible by clicking on system tray icon or running `copyq toggle`.

To exit the application select Exit from tray menu or press Ctrl-Q keys in the
application window.

## Developers and Translators

If you want to help with translating, fixing or writing code read
[HACKING](https://github.com/hluk/CopyQ/blob/master/HACKING) file.

## Dependencies

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

## Keyboard navigation

* `PgDown/PgUp`, `Home/End`, `Up/Down` - item list navigation
* `Left`, `Right`, `Ctrl+Tab`, `Ctrl+Shift+Tab` - tab navigation
* `Ctrl+T`, `Ctrl+W` - create and remove tabs
* `Ctrl+Up`, `Ctrl+Down` - move selected items
* `Ctrl+Left`, `Ctrl+Right` - cycle through item formats
* `Esc` - cancel search, hide window
* `Ctrl+Q` - exit
* `F2` - edit selected items
* `Ctrl+E` - edit items in an external editor
* `F5` - open action dialog for selected items
* `Delete` - delete selected items
* `Ctrl+A` - select all
* `Enter` - put current item into clipboard and paste item (optional)
* `F1` - show help

Start typing a text to search items.

On OS X, use Command instead of Ctrl for the shortcuts above.

## Basic Usage

To start CopyQ run `copyq` from command line or just launch the application from
menu or installed location.

Application can be accessed from tray or by restoring minimized window
if tray is unavailable.

Copying text or image to clipboard will create new item in the list.
Selected items can be:
* edited (`F2`),
* removed (`Delete`),
* sorted (`Ctrl+Shift+S`, `Ctrl+Shift+R`),
* moved around (with mouse or `Ctrl+Up/Down`) or
* copied back to clipboard (`Enter`, `Ctrl+V`).

All items will be restored when application is started next time.

To create custom action that can be triggered from menu or with
shortcut, go to Command dialog `F6`, click Add button and select
predefined command or create new one.
One of very useful predefined commands there is "Show/hide main window".

## Command Line

CopyQ has powerful command line and scripting interface.

Note: The main application must be running to be able to issue commands using
command line.

Print help for some useful command line arguments:

    copyq --help
    copyq --help add
    copyq --help tab

Insert some texts to the history:

    copyq add "first item" "second item" "third item"

Print content of the first three items:

    copyq read 0 1 2
    copyq separator "," read 0 1 2

Show current clipboard content:

    copyq clipboard
    copyq clipboard text/html

Show formats available in clipboard:

    copyq clipboard \?

Copy text to the clipboard:

    copyq copy "Some Text"

Load file content into clipboard:

    copyq copy - < file.txt
    copyq copy text/html < index.html
    copyq copy image/jpeg - < image.jpg

Create an image items:

    copyq write image/gif - < image.gif
    copyq write image/svg - < image.svg

Create items for matching files in a directory (in this case
`$HOME/Documents/*.doc`):

    copyq action 'copyq: home = Dir().home(); home.cd("Documents") && home.entryList(["*.doc"])'

Show notification with clipboard content:

    copyq eval 'copyq: popup("Clipboard", clipboard())'
    copyq clipboard | copyq popup Clipboard -

For more advanced usage see [Wiki](https://github.com/hluk/CopyQ/wiki) and
[Scripting Reference](https://github.com/hluk/CopyQ/blob/master/src/scriptable/README.md)


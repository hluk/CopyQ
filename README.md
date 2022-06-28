# CopyQ

[![Documentation Status](https://readthedocs.org/projects/copyq/badge/?version=latest)](https://copyq.readthedocs.io/en/latest/?badge=latest)
[![Translation Status](https://hosted.weblate.org/widgets/copyq/-/svg-badge.svg)](https://hosted.weblate.org/engage/copyq/?utm_source=widget)
[![Linux Build Status](https://github.com/hluk/CopyQ/workflows/Linux%20Build/badge.svg?branch=master&event=push)](https://github.com/hluk/CopyQ/actions?query=branch%3Amaster+event%3Apush+workflow%3A%22Linux+Build%22)
[![macOS Build Status](https://github.com/hluk/CopyQ/workflows/macOS%20Build/badge.svg?branch=master&event=push)](https://github.com/hluk/CopyQ/actions?query=branch%3Amaster+event%3Apush+workflow%3A%22macOS+Build%22)
[![Windows Build Status](https://ci.appveyor.com/api/projects/status/github/hluk/copyq?branch=master&svg=true)](https://ci.appveyor.com/project/hluk/copyq)
[![Coverage Status](https://coveralls.io/repos/hluk/CopyQ/badge.svg?branch=master)](https://coveralls.io/r/hluk/CopyQ?branch=master)

CopyQ is an advanced clipboard manager with powerful editing and scripting features.

- [Downloads](https://github.com/hluk/CopyQ/releases)
- [Web Site](https://hluk.github.io/CopyQ/)
- [Documentation](https://copyq.readthedocs.io)
- [Mailing List](https://groups.google.com/group/copyq)
- [Bug Reports](https://github.com/hluk/CopyQ/issues)
- [Donate](https://liberapay.com/CopyQ/)
- [Scripting API](https://copyq.readthedocs.io/en/latest/scripting-api.html)

## Overview

* CopyQ monitors the system clipboard and saves its content in customized tabs.
* Saved clipboard entries can later be copied and pasted directly into any application.

## Features

* Support for Linux, Windows, and [OS X 10.15+](https://doc.qt.io/qt-5/macos.html#supported-versions)
* Store text, HTML, images, and any other custom formats
* Quickly browse and filter items in clipboard history
* Sort, create, edit, remove, copy/paste, drag'n'drop items in tabs
* Add notes and tags to items
* System-wide keyboard shortcuts with customizable commands
* Paste items with keyboard shortcuts, from tray, or from main window
* Fully customizable appearance
* Advanced command-line interface and scripting
* Ignore clipboard copied from specified windows or containing specified text
* Support for simple Vim-like editor with keyboard shortcuts
* Many more features

## Install

<a href="https://repology.org/metapackage/copyq">
    <img src="https://repology.org/badge/vertical-allrepos/copyq.svg" alt="Packaging status" align="right">
</a>

To install CopyQ, use the binary package or the installer provided for your operating system.

For unlisted operating systems, please follow the instructions in
[Build from Source Code](https://copyq.readthedocs.io/en/latest/build-source-code.html).

### Windows

[![Chocolatey package](https://repology.org/badge/version-for-repo/chocolatey/copyq.svg)](https://repology.org/metapackage/copyq)

On Windows you can use any of the following options to install CopyQ:

* [Installer (setup.exe)](https://github.com/hluk/CopyQ/releases)
* [Portable zip package](https://github.com/hluk/CopyQ/releases)
* [Scoop package](https://scoop.sh/) from the [extras bucket](https://github.com/lukesampson/scoop-extras).
* [Chocolatey package](https://chocolatey.org/packages/copyq)

Using Scoop:

```
scoop install copyq
```

Using Chocolatey:

```
choco install copyq
```

### OS X

[![Homebrew package](https://repology.org/badge/version-for-repo/homebrew_casks/copyq.svg)](https://repology.org/metapackage/copyq)

On OS X you can use [Homebrew](https://brew.sh/) to install CopyQ:

```bash
brew install --cask copyq
```

### Debian 10+, Ubuntu 18.04+, and their derivatives

Install `copyq` and `copyq-plugins` packages.

#### Ubuntu PPA

Install and keep CopyQ always up to date by running the following commands from
the terminal (the package from PPA contains all plugins and documentation):

```bash
sudo add-apt-repository ppa:hluk/copyq
sudo apt update
sudo apt install copyq
```

### Fedora

Install `copyq` package.

### Arch Linux

Install `copyq` package.

### Other Linux Distributions

Install [Flatpak](https://www.flatpak.org/) and `com.github.hluk.copyq` from
[Flathub](https://flathub.org/).

```bash
flatpak install flathub com.github.hluk.copyq
```

Start CopyQ from the menu or with the following command:

```bash
flatpak run com.github.hluk.copyq
```

## Using the App

To start CopyQ, double-click the program icon or run `copyq`.

The list with the clipboard history is accessible by clicking on the system tray icon
or by running `copyq toggle`.

Copying text or image to the clipboard will create a new item in the list.

Selected items can be:

* edited (`F2`)
* removed (`Delete`)
* sorted (`Ctrl+Shift+S`, `Ctrl+Shift+R`)
* repositioned (with mouse or `Ctrl+Up/Down`)
* copied back to the clipboard (`Ctrl+C`)
* pasted to the previously active window (`Enter`)

All items will be restored when CopyQ is next started.

To exit CopyQ, select Exit from the tray menu or press `Ctrl-Q` in the
CopyQ window.

Read more:

- [Basic Usage](https://copyq.readthedocs.io/en/latest/basic-usage.html)
- [Keyboard](https://copyq.readthedocs.io/en/latest/keyboard.html)

### Adding Functionality

To create custom actions that can be executed
from the menu, with keyboard shortcuts, or when the clipboard changes:
- go to the Command dialog (`F6` shortcut)
- click the `Add` button, then select a predefined command, or create a new one
- optionally change the command details (shortcut, name)
- click `OK` to save the command

One of the very useful predefined commands is "Show/hide main window".

Read more:

- [Writing Commands](https://copyq.readthedocs.io/en/latest/writing-commands-and-adding-functionality.html)
- [CopyQ Commands Repository](https://github.com/hluk/copyq-commands)

### Command Line

CopyQ has a powerful command line and scripting interface.

Note: The main application must be running to be able to issue commands using the
command line.

Print help for some useful command line arguments:

    copyq --help
    copyq --help add

Insert some text in the history:

    copyq add -- 'first item' 'second item' 'third item'

Omitting the double-dash (`--`) in the command above would mean that slashes
(`\`) in arguments will be treated as special characters.  For example, `\n` will be treated as 
the new line character, `\t` as tab, `\\` as slash, `\x` as `x`, etc.

Create a single item containing two lines:

    copyq add 'first line\nsecond line'

Print the content of the first three items:

    copyq read 0 1 2
    copyq separator "," read 0 1 2

Show the current clipboard content:

    copyq clipboard
    copyq clipboard text/html
    copyq clipboard \?    # lists formats in clipboard

Copy text to the clipboard:

    copyq copy "Some Text"

Load file content into the clipboard:

    copyq copy - < file.txt
    copyq copy text/html < index.html
    copyq copy image/jpeg - < image.jpg

Create image items:

    copyq write image/gif - < image.gif
    copyq write image/svg - < image.svg

Read more:

- [Scripting](https://copyq.readthedocs.io/en/latest/scripting.html)
- [Scripting API](https://copyq.readthedocs.io/en/latest/scripting-api.html)

## Build from Source Code

To build CopyQ from source code, first install the required dependencies:

- [Git](https://git-scm.com/)
- [CMake](https://cmake.org/download/)
- [Qt](https://download.qt.io/archive/qt/)
- optional on Linux/X11: development files and libraries for [Xtst](https://t2-project.org/packages/libxtst.html) and [Xfixes](https://www.x.org/archive/X11R7.5/doc/man/man3/Xfixes.3.html)

### Install Dependencies

#### Ubuntu

```bash
sudo apt install \
  cmake \
  extra-cmake-modules \
  git \
  libqt5svg5 \
  libqt5svg5-dev \
  libqt5waylandclient5-dev \
  libqt5x11extras5-dev \
  libwayland-dev \
  libxfixes-dev \
  libxtst-dev \
  qtbase5-private-dev \
  qtdeclarative5-dev \
  qttools5-dev \
  qttools5-dev-tools \
  qtwayland5 \
  qtwayland5-dev-tools
```
#### RHEL / CentOS / Oracle Linux

```bash
sudo yum install \
  cmake \
  extra-cmake-modules \
  gcc-c++ \
  git \
  libXfixes-devel \
  libXtst-devel \
  qt5-qtbase-devel \
  qt5-qtdeclarative-devel \
  qt5-qtsvg-devel \
  qt5-qttools-devel \
  qt5-qtwayland-devel \
  qt5-qtx11extras-devel \
  wayland-devel \
  kf5-knotifications-devel
```

### Build CopyQ

Change install prefix if needed:

```bash
git clone https://github.com/hluk/CopyQ.git
cd CopyQ
cmake .
make
```

You can now run the built app.

```bash
./copyq
```

## Contributions

You can help translate CopyQ (click the banner below)
or help [fix issues and implement new features](https://github.com/hluk/CopyQ/issues).

[![Translations](https://hosted.weblate.org/widgets/copyq/-/287x66-white.png)](https://hosted.weblate.org/engage/copyq/?utm_source=widget)

Read more:

- [Build from Source Code](https://copyq.readthedocs.io/en/latest/build-source-code.html)
- [Fixing Bugs and Adding Features](https://copyq.readthedocs.io/en/latest/fixing-bugs.html)
- [Translations](https://copyq.readthedocs.io/en/latest/translations.html)

Installation
============

Packages and installation files are available at `Releases page <https://github.com/hluk/CopyQ/releases>`__.

Alternatively, you can install the app with one of the following methods:

On **Windows**, you can install `Chocolatey package <https://chocolatey.org/packages/copyq>`__.

On **macOS**, you can use `Homebrew <https://brew.sh/>`__ to install the app
(also see the problem in the next section):

.. code-block:: bash

    brew install --cask copyq

On **macOS**, if you encounter an issue where the app crashes with a dialog
saying "CopyQ is damaged" or "CopyQ cannot be opened", you may need to run the
following commands (for details, see `issue #2652
<https://github.com/hluk/CopyQ/issues/2652>`__):

.. code-block:: bash

    xattr -d com.apple.quarantine /Applications/CopyQ.app
    codesign --force --deep --sign - /Applications/CopyQ.app

On Debian unstable, **Debian 11+**, **Ubuntu 22.04+** and later derivatives can
install stable version from official repositories:

.. code-block:: bash

    sudo apt install copyq
    # copyq-plugins and copyq-doc is split out and can be installed independently

On **Ubuntu** set up the official PPA repository and install the app from terminal:

.. code-block:: bash

    sudo apt install software-properties-common python-software-properties
    sudo add-apt-repository ppa:hluk/copyq
    sudo apt update
    sudo apt install copyq
    # this package contains all plugins and documentation

On **Fedora**, install "copyq" package:

.. code-block:: bash

    sudo dnf install copyq

On other Linux distributions, you can use `Flatpak <https://flatpak.org/>`__
to install the app:

.. code-block:: bash

    # Install from Flathub.
    flatpak install --user --from https://flathub.org/repo/appstream/com.github.hluk.copyq.flatpakref

    # Run the app.
    flatpak run com.github.hluk.copyq

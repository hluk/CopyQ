Installation
============

Packages and installation files are available at `Releases page <https://github.com/hluk/CopyQ/releases>`__.

Alternatively, you can install the app with one of the following methods:

a. On **Windows**, you can install `Chocolatey package <https://chocolatey.org/packages/copyq>`__.
b. On **OS X**, you can use `Homebrew <https://brew.sh/>`__ to install the app.

.. code-block:: bash

    brew install --cask copyq

On Debian unstable, **Debian 10+**, **Ubuntu 18.04+** and later derivatives can
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

On other Linux distributions, you can use `Flatpak <https://www.flatpak.org/>`__
to install the app:

.. code-block:: bash

    # Install from Flathub.
    flatpak install --user --from https://flathub.org/repo/appstream/com.github.hluk.copyq.flatpakref

    # Run the app.
    flatpak run com.github.hluk.copyq

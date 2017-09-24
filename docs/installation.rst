Installation
============

Packages and installation files are available at `Releases page <https://github.com/hluk/CopyQ/releases>`__.
Alternatively you can install the app with one of the following methods.

On **Windows** you can install `Chocolatey package <https://chocolatey.org/packages/copyq>`__.

On **OS X** you can use `Homebrew <https://brew.sh/>`__ to install the app.

.. code-block:: bash

    brew cask install copyq

On Debian unstable, **Debian 10+**, **Ubuntu 18.04+** and later derivatives can
install stable version from official repositories:

.. code-block:: bash

    sudo apt install copyq
    # copyq-plugins and copyq-doc is splitted out and can be installed independently

On **Ubuntu** set up the official PPA repository and install the app from terminal.

.. code-block:: bash

    sudo apt install software-properties-common python-software-properties
    sudo add-apt-repository ppa:hluk/copyq
    sudo apt update
    sudo apt install copyq

Build from Source Code
======================

This page describes how to build the application from source code.

Get the Source Code
-------------------

Download the source code from git repository

::

    git clone https://github.com/hluk/CopyQ.git

or download the latest source code archive from:

- `latest release <https://github.com/hluk/CopyQ/releases>`__
- `master branch in zip <https://github.com/hluk/CopyQ/archive/master.zip>`__
- `master branch in tar.gz <https://github.com/hluk/CopyQ/archive/master.tar.gz>`__

Install Dependencies
--------------------

The build requires:

- `CMake <https://cmake.org/download/>`__
- `Qt <https://download.qt.io/archive/qt/>`__

Ubuntu
^^^^^^
On **Ubuntu** you can install all build dependencies with:

::

    sudo apt install \
      build-essential \
      cmake \
      extra-cmake-modules \
      git \
      libkf5notifications-dev \
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

Fedora / RHEL / Centos
^^^^^^^^^^^^^^^^^^^^^^
On **Fedora** and derivatives you can install all build dependencies with Qt 5:

::

    sudo yum install \
      cmake \
      extra-cmake-modules \
      gcc-c++ \
      git \
      kf5-knotifications-devel \
      libSM-devel \
      libXfixes-devel \
      libXtst-devel \
      qt5-qtbase-devel \
      qt5-qtbase-private-devel \
      qt5-qtdeclarative-devel \
      qt5-qtsvg-devel \
      qt5-qttools-devel \
      qt5-qtwayland-devel \
      qt5-qtx11extras-devel \
      wayland-devel

To build with Qt 6:

::

    sudo yum install \
      cmake \
      extra-cmake-modules \
      gcc-c++ \
      git \
      kf6-knotifications-devel \
      kf6-kstatusnotifieritem-devel \
      libSM-devel \
      libXfixes-devel \
      libXtst-devel \
      qt6-qtbase-devel \
      qt6-qtbase-private-devel \
      qt6-qtdeclarative-devel \
      qt6-qtsvg-devel \
      qt6-qttools-devel \
      qt6-qtwayland-devel \
      wayland-devel

Build and Install
-----------------

Build the source code with CMake and make or using an IDE of your choice (see next sections).

::

    cd CopyQ
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local .
    make
    make install

Qt Creator
----------

Qt Creator is IDE focused on developing C++ and Qt applications.

Install Qt Creator from your package manager or by selecting it from Qt installation utility.

Set up Qt library, C++ compiler and CMake.

.. seealso::

    `Adding Kits <https://doc.qt.io/qtcreator/creator-targets.html>`__

Open file ``CMakeLists.txt`` in repository clone to create new project.

Visual Studio
-------------

You need to install Qt for given version Visual Studio.

In Visual Studio 2017 open folder containing repository clone using "File - Open - Folder".

In older versions, create solution manually by running ``cmake -G "Visual Studio 14 2015 Win64" .``
(select appropriate generator name) in repository clone folder.

.. seealso::

    `CMake - Visual Studio Generators <https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#visual-studio-generators>`__

Building and Packaging for OS X
-------------------------------

On OS X, required Qt 5 libraries and utilities can be easily installed with `Homebrew <https://brew.sh/>`__.

::

    cd CopyQ
    git -C "utils/github/homebrew" init .
    git -C "utils/github/homebrew" add .
    git -C "utils/github/homebrew" commit -m "Initial"
    
    brew tap copyq/kde utils/github/homebrew/
    # if the above "brew tap" command produces an error like
    #     "Error: Tap copyq/kde remote mismatch"
    # then run
    #     brew untap --force copyq/kde
    # and re-run the above "brew tap" command

    brew install qt6 copyq/kde/kf6-knotifications copyq/kde/kf6-kstatusnotifieritem

Build with the following commands:

::

    cmake -DCMAKE_PREFIX_PATH="$(brew --prefix qt5)" .
    cmake --build .
    cpack

This will produce a self-contained application bundle ``CopyQ.app``
which can then be copied or moved into ``/Applications``.

NOTE: If no Items are shown when you start CopyQ and open "File - Preferences - Items",
then your CopyQ plugins were not installed. If you saw warning messages like this::

     /<some_path>/install_name_tool: warning: changes being made to the file will invalidate the code signature in: /<some_path>/CopyQ/_CPack_Packages/Darwin/DragNDrop/copyq-6.2.0-Darwin/CopyQ.app/Contents/Plugins/<some_file>.dylib

when you ran the above "cpack" command, then you have likely encountered
`issue 1903 <https://github.com/hluk/CopyQ/issues/1903/>`__.

In that case you may codesign the CopyQ app again using the following command,
un-install the previous CopyQ app, and install the re-signed ``CopyQ.app``::

    codesign --force --deep --sign - $PWD/_CPack_Packages/Darwin/DragNDrop/copyq-*-Darwin/CopyQ.app


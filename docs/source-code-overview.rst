Source Code Overview
====================

This page describes application processes and source code.

Applications, Frameworks and Libraries
--------------------------------------

The application is written in C++17 and uses Qt framework.

Source code can be build either with CMake.

Most icons in the application are taken from theme by default (which
currently works only on Linux) with fallback to built-in icons provided
by `FontAwesome <http://fontawesome.io/>`__.

The application logo and icons were created in `Inkscape
<https://inkscape.org/>`__ (icon source is in `src/images/icon.svg
<https://github.com/hluk/CopyQ/blob/master/src/images/icon.svg>`__).

Application Processes
---------------------

There are these system processes related to CopyQ:

- Main GUI application
- Clipboard monitor - executes automatic clipboard commands
- Menu command filter - enables/hides custom menu items based on "Filter" field
  in menu commands
- Display command - executes display commands as needed
- Clipboard and X11 selection owner and synchronization - provides clipboard
  data; launched as needed
- Multiple clients - anything run by user from Action dialog or triggered as
  menu, automatic or global-shortcut command

Main GUI Application
~~~~~~~~~~~~~~~~~~~~

The main GUI application (or server) can be executed by running
``copyq`` binary without attributes (session name can be optionally
specified on command line).

It creates local server allowing communication with clipboard monitor
process and other client processes.

Each user can run multiple main application processes each with unique
session name (default name is empty).

Clipboard Monitor
~~~~~~~~~~~~~~~~~

Clipboard monitoring happens in separate process because otherwise it
would block GUI (in Qt clipboard needs to be accessed in main GUI
thread). The process is allowed to crash or loop indefinitely due to
bugs on some platforms.

Setting and retrieving clipboard can still happen in GUI thread (copying
and pasting in various GUI widgets) but it's preferred to send and
receive clipboard data using monitor process.

The monitor process is launched as soon as GUI application starts and is
restarted whenever it doesn't respond to keep-alive requests.

Clients and Scripting
~~~~~~~~~~~~~~~~~~~~~

Scripting language is `Qt
Script <https://doc.qt.io/qt-6/qtqml-javascript-functionlist.html>`__ (mostly
same syntax and functions as JavaScript).

API is described in :ref:`scripting-api`.

A script can be started by passing arguments to ``copyq``.
For example: ``copyq "1+1"``

After script finishes, the server sends back output of last command and
exit code (non-zero if script crashes).

.. code-block:: bash

    copyq eval 'read(0,1,2)' # prints first three items in list
    copyq eval 'fail()' # exit code will be non-zero

While script is running, it can send print requests to client.

.. code-block:: bash

    copyq eval 'print("Hello, "); print("World!\n")'

Scripts can ask for stdin from client.

.. code-block:: bash

    copyq eval 'var client_stdin = input()'

The script run in current directory of client process.

.. code-block:: bash

    copyq eval 'Dir().absolutePath()'
    copyq eval 'execute("ls", "-l").stdout'

Single function call where all arguments are numbers or strings can be
executed by passing function name and function arguments on command
line. Following commands are equal.

.. code-block:: bash

    copyq eval 'copy("Hello, World!")'
    copyq copy "Hello, World!"

Getting application version or help mustn't require the server to be
running.

.. code-block:: bash

    copyq help
    copyq version

Scripts run in separate thread and communicate with main thread by
calling methods on an object of ``ScriptableProxy`` class. If called
from non-main thread, these methods invoke a slot on an ``QObject`` in
main thread and pass it a function object which simply calls the method
again.

.. code-block:: cpp

    bool ScriptableProxy::loadTab(const QString &tabName)
    {
        // This section is wrapped in an macro so to remove duplicate code.
        if (!m_inMainThread) {
            // Callable object just wraps the lambda so it's possible to send it to a slot.
            auto callable = createCallable([&]{ return loadTab(tabName); });

            m_inMainThread = true;
            QMetaObject::invokeMethod(m_wnd, "invoke", Qt::BlockingQueuedConnection, Q_ARG(Callable*, &callable));
            m_inMainThread = false;

            return callable.result();
        }

        // Now it's possible to call method on an object in main thread.
        return m_wnd->loadTab(tabName);
    }

Platform-dependent Code
-----------------------

Code for various platforms is stored in
`src/platform <https://github.com/hluk/CopyQ/tree/master/src/platform>`__.

This leverages amount of ``#if``\ s and similar preprocessor directives
in common code.

Each supported platform implements
`PlatformNativeInterface <https://github.com/hluk/CopyQ/blob/master/src/platform/platformnativeinterface.h>`__
and ``platformNativeInterface()``.

The implementations can contain:

-  Creating Qt application objects
-  Clipboard handling (for clipboard monitor)
-  Focusing window and getting window titles
-  Getting system paths
-  Setting "autostart" option
-  Handling global shortcuts (**note:** this part is in
   `qxt/ <https://github.com/hluk/CopyQ/tree/master/qxt>`__)

For unsupported platforms there is `simple
implementation <https://github.com/hluk/CopyQ/tree/master/src/platform/dummy>`__
to get started.

Plugins
-------

Plugins are built as dynamic libraries which are loaded from runtime
plugin directory (platform-dependent) after application start.

Code is stored in
`plugins <https://github.com/hluk/CopyQ/tree/master/plugins>`__.

Plugins implement interfaces from
`src/item/itemwidget.h <https://github.com/hluk/CopyQ/tree/master/src/item/itemwidget.h>`__.

To create new plugin just duplicate and rewrite an existing plugin. You
can build the plugin with ``make {PLUGIN_NAME}``.

Continuous Integration (CI)
---------------------------

The application binaries and packages are built and tested on multiple
CI servers.

-  `GitHub Actions <https://github.com/hluk/CopyQ/actions>`__
    -  Builds packages for OS X.
    -  Builds and runs tests for Linux binaries.

-  `GitLab CI <https://gitlab.com/CopyQ/CopyQ/builds>`__
    -  Builds and runs tests for Ubuntu 22.04 binaries.
    -  Screenshots are taken while GUI tests are running. These are
       available if a test fails.

-  `AppVeyor <https://ci.appveyor.com/project/hluk/copyq>`__
    -  Builds installers and portable packages for Windows.
    -  Provides downloads for recent commits.
    -  Release build are based on gcc-compiled binaries (Visual Studio
       builds are also available).

-  `OBS Linux Packages <https://build.opensuse.org/project/show/home:lukho:copyq>`__
    -  Builds release packages for various Linux distributions.

-  `Beta OBS Linux Packages <https://build.opensuse.org/project/show/home:lukho:copyq-beta>`__
    -  Builds beta and unstable packages for various Linux distributions.

-  `Codecov <https://app.codecov.io/gh/hluk/CopyQ>`__
    -  Contains coverage report from tests run with GitHub Actions.

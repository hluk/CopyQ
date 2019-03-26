Fixing Bugs and Adding Features
===============================

This page describes how to build, fix and improve the source code.

Making Changes
--------------

Pull requests are welcome at `github project
page <https://github.com/hluk/CopyQ>`__.

For more info see `Creating a pull request from a fork <https://help.github.com/articles/creating-a-pull-request-from-a-fork/>`__.

Try to keep the code style consistent with the existing code.

Build the Debug Version
-----------------------

.. code-block:: bash

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=ON ..
    make

Run Tests
---------

You can run automated tests if the application is built either in debug
mode, with CMake flag ``-DWITH_TESTS=ON``.

Run the tests with following command.

.. code-block:: bash

    copyq tests

This command will execute all test cases in new special CopyQ session so
that user configuration, tabs and items are not modified. It's better to
close any other CopyQ session before running tests since they can affect
test results.

While running tests there must be **no keyboard and mouse interaction**.
Preferably you can execute the tests in separate virtual environment. On
Linux you can run the tests on virtual X11 server with ``xvfb-run``.

.. code-block:: bash

    xvfb-run sh -c 'openbox & sleep 1; copyq tests'

Test invocation examples:

- Print help for tests: ``copyq tests --help``
- Run specific tests: ``copyq tests commandHelp commandVersion``
- Run specific tests for a plugin: ``copyq tests 'PLUGINS:pinned' isPinned``
- Run tests only for specific plugins: ``copyq tests 'PLUGINS:pinned|tags'``
- List tests: ``copyq tests -functions``
- List tests for a plugin: ``copyq tests PLUGINS:tags -functions``
- Less verbose tests: ``copyq tests -silent``
- Slower GUI tests: ``COPYQ_TESTS_KEYS_WAIT=1000 COPYQ_TESTS_KEY_DELAY=50 copyq tests editItems``

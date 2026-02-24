## Commands

Always use the following environment variables for all `build/copyq` and
`build/copyq-tests` commands:

    export COPYQ_SESSION_NAME="test"
    export COPYQ_SETTINGS_PATH="build/copyq-test-conf"
    export COPYQ_ITEM_DATA_PATH="build/copyq-test-data"
    export COPYQ_PLUGINS=""
    export COPYQ_DEFAULT_ICON="1"
    export COPYQ_SESSION_COLOR="#f90"
    export COPYQ_THEME_PREFIX="$PWD/shared/themes"
    export COPYQ_PASSWORD="TEST123"
    export COPYQ_LOG_LEVEL="DEBUG"
    export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false"
    export QT_QPA_PLATFORM="xcb"

Run CMake to configure build:

    cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_INSTALL_PREFIX=$PWD/build/install \
      -DCMAKE_CXX_FLAGS="-ggdb -fdiagnostics-color" \
      -DWITH_TESTS=ON \
      -DPEDANTIC=ON .

Build: `cmake -B build --build`

Install: `cmake -B build --target install`

Tests require X11 session (or Wayland) and a window manager. Use `Xvfb` and
`openbox` to initialize the testing environment.

Avoid running all tests, always specify a list tests functions to run.

Run tests after build: `build/copyq-tests $TEST_FUNCTIONS`

Run tests for specific plugin: `build/copyq-tests PLUGINS:sync $TEST_FUNCTIONS`

List tests function names: `build/copyq-tests -functions`

List tests function names for a plugin: `build/copyq-tests PLUGINS:image -functions`

Start the server process: `build/copyq`

In case any process exits with exit code 11 (SIGSEGV) use `coredumpctl` utility
to find the root cause.

Stop the server process: `build/copyq exit`

List server and client logs (server process does not need to run): `build/copyq logs`

Run a script - requires server to be running:

    build/copyq source script.js

    # the above command is equivalent to
    build/copyq 'source("script.js")'

Scripting API documentation is in @docs/scripting-api.rst.

Useful scripts (omit the `tab(...)` call to use the default tab):

- `tab('TAB1'); add('ITEM')` - prepend ITEM text item to the TAB1 tab
- `tab('TAB1'); size()` - item count in the TAB1 tab
- `tab('TAB1'); read(0,1,2)` - read items at indexes 0, 1 and 2 in the TAB1 tab
- `config()` - list configuration options with current value and description
- `config('check_clipboard', 'false')` - set an option

## Project structure

- @plugins - code for various plugins build as dynamic modules loaded optionally by the app
- @src - main app code
- @src/app - wrappers for QCoreApplication object
- @src/common - common functionality, client/server local socket handling, logging
- @src/gui - GUI widgets and some helper modules
- @src/item - tab and item data handling, serialization code
- @src/platform - platform-specific code
- @src/scriptable - scripting capabilities
- @src/tests - tests for the main app
- @src/ui - Qt widget definition files (XML)
- @qxt - code to handle global system-wide shortcuts

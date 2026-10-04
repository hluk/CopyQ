## Commands

Always use the environment variables from `utils/env.sh` for all `build/copyq`
and `build/copyq-tests` commands:

    source utils/env.sh

Run CMake to configure build:

    cmake -B build -DWITH_TESTS=ON -DPEDANTIC=ON .

Build: `cmake -B build --build`

Install: `cmake -B build --target install`

**Start Xvfb and openbox (or a Wayland compositor) before running any
`build/copyq` or `build/copyq-tests` command**, otherwise the process will
crash (exit code 134 or SIGSEGV):

- X11 setup: `source utils/xvfb.sh`
- Wayland setup: `USE_KWIN=1 source utils/wayland.sh`

Avoid running all tests, always specify a list of test functions to run
(`group:tag`):

    build/copyq-tests testCore:configPath testCore:badCommand

List test groups and tags: `build/copyq-tests -datatags`

Run tests matching a substring: `COPYQ_TESTS_FILTER=clipboard build/copyq-tests`

Start the server process: `build/copyq`

In case any process exits with exit code 11 (SIGSEGV) use `coredumpctl` utility
to find the root cause.

List server and client logs (server process does not need to run): `build/copyq logs`

Run a script - requires server to be running:

    build/copyq source script.js

    # the above command is equivalent to
    build/copyq 'source("script.js")'

Scripting API documentation is in @docs/scripting-api.rst. After changing it,
run @utils/script_docs_to_cpp.py to update the completion popup in the GUI.

Useful scripts:

- `tab('TAB1'); add('ITEM')` - prepend ITEM text item to the TAB1 tab
- `tab('TAB1'); size()` - TAB1 item count
- `read(0,1,2)` - read items at indexes 0, 1 and 2 (default tab)
- `config` - list configuration options with current value and description
- `config('check_clipboard', 'false')` - set an option
- `stats` - list QObjects, plugins, disk and memory usage etc

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

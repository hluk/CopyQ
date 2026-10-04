#!/bin/sh
# Setup environment for development: source utils/env.sh
export COPYQ_SESSION_NAME=test
export COPYQ_SETTINGS_PATH=build/copyq-test-conf
export COPYQ_ITEM_DATA_PATH=build/copyq-test-data
export COPYQ_STATE_PATH=build/copyq-test-conf
export COPYQ_PLUGINS=
export COPYQ_DEFAULT_ICON=1
export COPYQ_SESSION_COLOR="#f90"
export COPYQ_THEME_PREFIX="$PWD/shared/themes"
export COPYQ_PASSWORD=TEST123
export COPYQ_LOG_LEVEL=DEBUG

export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false"
export QT_QPA_PLATFORM=xcb

export CMAKE_BUILD_TYPE=Debug
export CMAKE_EXPORT_COMPILE_COMMANDS=1
export CMAKE_INSTALL_PREFIX="$PWD/build/install"
export CMAKE_CXX_FLAGS="-ggdb -fdiagnostics-color"
export CMAKE_GENERATOR=Ninja

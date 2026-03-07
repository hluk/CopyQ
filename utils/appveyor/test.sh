#!/usr/bin/bash
set -exuo pipefail

# shellcheck disable=SC1091
source utils/appveyor/env.sh

export PATH="$GPGPATH":$Destination:$PATH
mkdir ~/.gnupg
chmod go-rwx ~/.gnupg
gpg --version

cp -v \
    "$BUILD_PATH/${BUILD_SUB_DIR:-}/copyq-tests.exe" \
    "$QTDIR/bin/Qt6Test.dll" \
    "$Destination"

export COPYQ_PLUGINS=$BUILD_PATH/src/${BUILD_SUB_DIR:-}/itemtests.dll

# Skip platform-independent tests already covered by Linux CI.
# This reduces Windows test runtime from ~13 min to ~5 min.
# These tests exercise pure scripting/data logic with no OS-specific behavior
# (no clipboard, window focus, tray, shortcuts, or dialog interaction).
SKIP_TESTS=(
    # Scripting engine
    commandEval
    commandEvalThrows
    commandEvalSyntaxError
    commandEvalArguments
    commandEvalEndingWithComment
    commandPrint
    commandAbort
    commandFail
    commandSource
    commandsUnicode
    commandSleep
    commandDateString
    commandAfterMilliseconds
    commandMimeTypes
    commandServerLogAndLogs
    commandEscapeHTML
    commandExecute

    # Data operations
    commandsAddRead
    commandsWriteRead
    commandsReadUtf8ByDefault
    commandChange
    commandsPackUnpack
    commandsBase64
    commandsGetSetItem
    commandsChecksums
    commandsData
    commandsEnvSetEnv
    insertRemoveItems
    handleUnexpectedTypes

    # Config and settings
    commandConfig
    commandToggleConfig
    commandSettings
    commandSetCurrentTab
    commandGetSetCurrentPath
    commandLoadTheme
    configMaxitems
    configAutostart
    envVariablePaths
    configTabs

    # Tab management
    tabAdd
    tabRemove
    tabIcon
    renameTab
    renameClipboardTab
    importExportTab
    setTabName

    # Import/export via scripting
    commandsExportImport
    commandsGetSetCommands
    commandsImportExportCommands
    commandsImportExportCommandsFixIndentation
    commandsAddCommandsRegExp

    # Class/object tests
    classByteArray
    classFile
    classDir
    classTemporaryFile
    classItemSelection
    classItemSelectionByteArray
    classItemSelectionSort
    classSettings
    calledWithInstance

    # CLI basics
    configPath
    readLog
    rotateLog
    pluginsDisabled
    commandHelp
    commandVersion
    badCommand
    badSessionName
    commandCatchExceptions

    # Script commands
    scriptCommandLoaded
    scriptCommandAddFunction
    scriptCommandOverrideFunction
    scriptCommandEnhanceFunction
    scriptCommandEndingWithComment
    scriptCommandWithError
    scriptOnItemsRemoved
    scriptOnItemsAdded
    scriptOnItemsChanged
    scriptEventMaxRecursion

    # Misc platform-independent
    pipingCommands
    action
    synchronizeInternalCommands
    displayCommand
    abortInputReader
    networkTests
    pluginNotInstalled
    startServerAndRunCommand
    expireTabs
)

# Build an associative array for O(1) lookup.
declare -A skip_set
for t in "${SKIP_TESTS[@]}"; do
    skip_set[$t]=1
done

# Get all core test names from the binary, subtract the skip list,
# and pass the remaining tests as positional arguments.
all_tests=$("$Destination/copyq-tests.exe" -functions 2>&1 | sed -n 's/()$//p')
run_tests=()
for t in $all_tests; do
    if [[ -z ${skip_set[$t]:-} ]]; then
        run_tests+=("$t")
    fi
done

echo "Running ${#run_tests[@]} of $(echo "$all_tests" | wc -w) core tests (skipping ${#SKIP_TESTS[@]} platform-independent tests)"

# Run selected core tests first. Passing test names as arguments disables
# plugin tests (see tests.cpp), so we run plugins separately below.
"$Destination/copyq-tests.exe" "${run_tests[@]}"

# Run all plugin tests (these exercise Windows-specific plugin behavior).
"$Destination/copyq-tests.exe" PLUGINS:.

gpgconf --kill all

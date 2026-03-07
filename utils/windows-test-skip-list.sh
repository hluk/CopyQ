#!/usr/bin/bash
# Platform-independent tests to skip on Windows CI.
# These are fully covered by Linux CI and exercise no OS-specific behavior
# (no clipboard, window focus, tray, shortcuts, or dialog interaction).
#
# Sourced by: utils/github/test-windows.sh

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

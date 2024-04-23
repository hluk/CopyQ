// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TESTS_H
#define TESTS_H

#include "tests/testinterface.h"

#include <QObject>
#include <QStringList>

class QProcess;
class QByteArray;

/**
 * Tests for the application.
 */
class Tests final : public QObject
{
    Q_OBJECT

public:
    explicit Tests(const TestInterfacePtr &test, QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void readLog();
    void commandHelp();
    void commandVersion();
    void badCommand();
    void badSessionName();

    void commandCatchExceptions();

    void commandExit();
    void commandEval();
    void commandEvalThrows();
    void commandEvalSyntaxError();
    void commandEvalArguments();
    void commandEvalEndingWithComment();
    void commandPrint();
    void commandAbort();
    void commandFail();
    void commandSource();

    void commandVisible();
    void commandToggle();
    void commandHide();
    void commandShow();
    void commandShowAt();
    void commandFocused();

    void commandsUnicode();

    void commandsAddRead();
    void commandsWriteRead();
    void commandChange();

    void commandSetCurrentTab();

    void commandConfig();
    void commandToggleConfig();

    void commandDialog();
    void commandDialogCloseOnDisconnect();

    void commandMenuItems();

    void commandsPackUnpack();
    void commandsBase64();
    void commandsGetSetItem();

    void commandsChecksums();

    void commandEscapeHTML();

    void commandExecute();

    void commandSettings();

    void commandsEnvSetEnv();

    void commandSleep();

    void commandsData();

    void commandCurrentWindowTitle();

    void commandCopy();
    void commandClipboard();
    void commandHasClipboardFormat();

    void commandEdit();
    void commandEditItem();

    void commandGetSetCurrentPath();

    void commandSelectItems();

    void commandsExportImport();

    void commandsGetSetCommands();

    void commandsImportExportCommands();
    void commandsImportExportCommandsFixIndentation();

    void commandsAddCommandsRegExp();

    void commandScreenshot();

    void commandNotification();

    void commandIcon();
    void commandIconTag();
    void commandIconTagColor();
    void commandLoadTheme();

    void commandDateString();

    void commandAfterMilliseconds();

    void commandAsync();

    void commandFilter();

    void commandMimeTypes();

    void commandUnload();
    void commandForceUnload();

    void commandServerLogAndLogs();

    void classByteArray();
    void classFile();
    void classDir();
    void classTemporaryFile();
    void classItemSelection();
    void classItemSelectionGetCurrent();
    void classItemSelectionByteArray();
    void classItemSelectionSort();
    void classSettings();
    void calledWithInstance();

    void pipingCommands();

    void chainingCommands();

    void configMaxitems();

    void keysAndFocusing();

    void selectItems();

    void moveItems();
    void deleteItems();
    void searchItems();
    void searchItemsAndSelect();
    void searchItemsAndCopy();
    void searchRowNumber();
    void searchAccented();
    void copyItems();
    void selectAndCopyOrder();

    void sortAndReverse();

    void createTabDialog();

    void copyPasteCommands();

    void editItems();
    void createNewItem();
    void editNotes();

    void toggleClipboardMonitoring();

    void clipboardToItem();
    void itemToClipboard();
    void tabAdd();
    void tabRemove();
    void tabIcon();
    void action();
    void insertRemoveItems();
    void renameTab();
    void renameClipboardTab();
    void importExportTab();

    void removeAllFoundItems();

    void nextPrevious();

    void externalEditor();

    void nextPreviousTab();

    void itemPreview();

    void openAndSavePreferences();

    void pasteFromMainWindow();

    void tray();
    void menu();

    void traySearch();
    void trayPaste();

    void pasteNext();

    // Options for tray menu.
    void configTrayTab();
    void configMove();
    void configTrayTabIsCurrent();

    void configAutostart();

    void configPathEnvVariable();
    void itemDataPathEnvVariable();

    void configTabs();

    void selectedItems();

    void shortcutCommand();
    void shortcutCommandOverrideEnter();
    void shortcutCommandMatchInput();
    void shortcutCommandMatchCmd();

    void shortcutCommandSelectedItemData();
    void shortcutCommandSetSelectedItemData();
    void shortcutCommandSelectedItemsData();
    void shortcutCommandSetSelectedItemsData();
    void shortcutCommandSelectedAndCurrent();
    void shortcutCommandMoveSelected();

    void automaticCommandIgnore();
    void automaticCommandRemove();
    void automaticCommandInput();
    void automaticCommandRegExp();
    void automaticCommandSetData();
    void automaticCommandOutputTab();
    void automaticCommandNoOutputTab();
    void automaticCommandChaining();
    void automaticCommandCopyToTab();
    void automaticCommandStoreSpecialFormat();
    void automaticCommandIgnoreSpecialFormat();

    void scriptCommandLoaded();
    void scriptCommandAddFunction();
    void scriptCommandOverrideFunction();
    void scriptCommandEnhanceFunction();
    void scriptCommandEndingWithComment();
    void scriptCommandWithError();

    void scriptPaste();
    void scriptOnTabSelected();
    void scriptOnItemsRemoved();
    void scriptOnItemsAdded();
    void scriptOnItemsChanged();
    void scriptOnItemsLoaded();
    void scriptEventMaxRecursion();
    void scriptSlowCollectOverrides();

    void displayCommand();
    void displayCommandForMenu();

    void synchronizeInternalCommands();

    void queryKeyboardModifiersCommand();
    void pointerPositionCommand();
    void setPointerPositionCommand();

    void setTabName();

    void showHideAboutDialog();
    void showHideClipboardDialog();
    void showHideItemDialog();
    void showHideLogDialog();
    void showHideActionHandlerDialog();

    void shortcutDialogAddShortcut();
    void shortcutDialogAddTwoShortcuts();
    void shortcutDialogChangeShortcut();
    void shortcutDialogSameShortcut();
    void shortcutDialogCancel();

    void actionDialogCancel();
    void actionDialogAccept();
    void actionDialogSelection();
    void actionDialogSelectionInputOutput();

    void exitConfirm();
    void exitNoConfirm();

    void abortInputReader();

    void changeAlwaysOnTop();

    void networkGet();
    void networkGetPostAsync();

    void pluginNotInstalled();

    void startServerAndRunCommand();

    void avoidStoringPasswords();

    void currentClipboardOwner();

    void saveLargeItem();

private:
    void clearServerErrors();
    int run(const QStringList &arguments, QByteArray *stdoutData = nullptr,
            QByteArray *stderrData = nullptr, const QByteArray &in = QByteArray(),
            const QStringList &environment = QStringList());
    bool hasTab(const QString &tabName);

    TestInterfacePtr m_test;
};

int runTests(int argc, char *argv[]);

#endif // TESTS_H

/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

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

    void commandExit();
    void commandEval();
    void commandEvalThrows();
    void commandEvalSyntaxError();
    void commandEvalArguments();
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

    void commandGetSetCurrentPath();

    void commandSelectItems();

    void commandsExportImport();

    void commandsGetSetCommands();

    void commandsImportExportCommands();
    void commandsImportExportCommandsFixIndentation();

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
    void calledWithBadInstance();

    void pipingCommands();

    void chainingCommands();

    void configMaxitems();

    void keysAndFocusing();

    void selectItems();

    void moveItems();
    void deleteItems();
    void searchItems();
    void searchItemsAndSelect();
    void searchRowNumber();
    void copyItems();

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

    // Options for tray menu.
    void configTrayTab();
    void configMove();
    void configTrayTabIsCurrent();

    void configAutostart();

    void configPathEnvVariable();

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
    void displayCommand();

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

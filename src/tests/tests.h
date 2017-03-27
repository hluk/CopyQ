/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

class RemoteProcess;
class QProcess;
class QByteArray;

/**
 * Tests for the application.
 */
class Tests : public QObject
{
    Q_OBJECT

public:
    explicit Tests(const TestInterfacePtr &test, QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void commandHelp();
    void commandVersion();
    void badCommand();

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

    void commandDialog();

    void commandsPackUnpack();
    void commandsBase64();
    void commandsGetSetItem();

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

    void commandScreenshot();

    void classFile();
    void classDir();
    void classTemporaryFile();

    void chainingCommands();

    void configMaxitems();

    void keysAndFocusing();

    void selectItems();

    void moveItems();
    void deleteItems();
    void searchItems();
    void searchRowNumber();
    void copyItems();

    void createTabDialog();

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

    void openAndSavePreferences();

    void pasteFromMainWindow();

    void tray();
    void menu();

    void traySearch();

    // Options for tray menu.
    void configTrayTab();
    void configMove();
    void configTrayTabIsCurrent();

    void shortcutCommand();
    void shortcutCommandOverrideEnter();
    void shortcutCommandMatchInput();
    void shortcutCommandMatchCmd();

    void shortcutCommandSelectedItemData();
    void shortcutCommandSetSelectedItemData();
    void shortcutCommandSelectedItemsData();
    void shortcutCommandSetSelectedItemsData();

    void automaticCommandIgnore();
    void automaticCommandInput();
    void automaticCommandRegExp();
    void automaticCommandSetData();
    void automaticCommandOutputTab();
    void automaticCommandNoOutputTab();

private:
    void clearServerErrors();
    int run(const QStringList &arguments, QByteArray *stdoutData = nullptr,
            QByteArray *stderrData = nullptr, const QByteArray &in = QByteArray());
    bool hasTab(const QString &tabName);

    TestInterfacePtr m_test;
};

int runTests(int argc, char *argv[]);

#endif // TESTS_H

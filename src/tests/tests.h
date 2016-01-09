/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
    explicit Tests(const TestInterfacePtr &test, QObject *parent = NULL);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void showHide();

    void windowTitle();

    void keysAndFocusing();

    void firstItemSelectedByDefault();

    void selectItems();

    void moveItems();
    void deleteItems();
    void searchItems();
    void copyItems();

    void helpCommand();
    void versionCommand();
    void badCommand();

    void copyCommand();

    void dialogCommand();

    void createAndCopyNewItem();

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
    void eval();
    void rawData();

    void nextPrevious();

    void options();

    void editCommand();

    void externalEditor();

    void editNotes();

    void exitCommand();
    void abortCommand();

    void nextPreviousTab();

    void openAndSavePreferences();

    void tray();

    void packUnpackCommands();
    void base64Commands();
    void getSetItemCommands();

    void escapeHTMLCommand();

    void executeCommand();

    void settingsCommand();

    void fileClass();
    void dirClass();

    void setEnvCommand();

private:
    void clearServerErrors();
    int run(const QStringList &arguments, QByteArray *stdoutData = NULL,
            QByteArray *stderrData = NULL, const QByteArray &in = QByteArray());
    bool hasTab(const QString &tabName);

    TestInterfacePtr m_test;
};

int runTests(int argc, char *argv[]);

#endif // TESTS_H

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

#ifndef ITEMSYNCTESTS_H
#define ITEMSYNCTESTS_H

#include "tests/testinterface.h"

#include <QObject>

class ItemSyncTests final : public QObject
{
    Q_OBJECT
public:
    explicit ItemSyncTests(const TestInterfacePtr &test, QObject *parent = nullptr);

    static QString testTab(int i);

    static QString testDir(int i);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void createRemoveTestDir();

    void itemsToFiles();
    void filesToItems();

    void removeOwnItems();
    void removeNotOwnedItems();
    void removeFiles();

    void modifyItems();
    void modifyFiles();

    void itemToClipboard();

    void notes();

    void customFormats();

    void getAbsoluteFilePath();

    void addItemsWhenFull();

    void addItemsWhenFullOmitDeletingNotOwned();

private:
    TestInterfacePtr m_test;
};

#endif // ITEMSYNCTESTS_H

// SPDX-License-Identifier: GPL-3.0-or-later

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
    void removeNotOwnedItemsCancel();
    void removeFiles();

    void modifyItems();
    void modifyFiles();

    void itemToClipboard();

    void notes();

    void customFormats();

    void getAbsoluteFilePath();

    void addItemsWhenFull();

    void addItemsWhenFullOmitDeletingNotOwned();

    void moveOwnItemsSortsBaseNames();

    void avoidDuplicateItemsAddedFromClipboard();

    void saveLargeItem();

private:
    TestInterfacePtr m_test;
};

#endif // ITEMSYNCTESTS_H

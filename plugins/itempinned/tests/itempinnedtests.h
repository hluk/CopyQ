// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "tests/testinterface.h"

#include <QObject>

class ItemPinnedTests final : public QObject
{
    Q_OBJECT
public:
    explicit ItemPinnedTests(const TestInterfacePtr &test, QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void isPinned();
    void pin();
    void pinMultiple();
    void unpin();
    void unpinMultiple();

    void removePinnedThrows();

    void pinToRow();

    void fullTab();

    void keepPinnedIfMaxItemsChanges();

private:
    TestInterfacePtr m_test;
};

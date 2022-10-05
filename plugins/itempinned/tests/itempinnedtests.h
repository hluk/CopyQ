// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMPINNEDTESTS_H
#define ITEMPINNEDTESTS_H

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

private:
    TestInterfacePtr m_test;
};

#endif // ITEMPINNEDTESTS_H

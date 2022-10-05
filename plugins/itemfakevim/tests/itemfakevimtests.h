// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMFAKEVIMTESTS_H
#define ITEMFAKEVIMTESTS_H

#include "tests/testinterface.h"

#include <QObject>

class ItemFakeVimTests final : public QObject
{
    Q_OBJECT
public:
    explicit ItemFakeVimTests(const TestInterfacePtr &test, QObject *parent = nullptr);

    static QString fileNameToSource();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void createItem();

    void blockSelection();

    void search();

    void incDecNumbers();

private:
    TestInterfacePtr m_test;
};

#endif // ITEMFAKEVIMTESTS_H

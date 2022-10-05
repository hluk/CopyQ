// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMTAGSTESTS_H
#define ITEMTAGSTESTS_H

#include "tests/testinterface.h"

#include <QObject>

class ItemTagsTests final : public QObject
{
    Q_OBJECT
public:
    explicit ItemTagsTests(const TestInterfacePtr &test, QObject *parent = nullptr);

    static QStringList testTags();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void userTags();
    void tag();
    void untag();
    void clearTags();
    void searchTags();

    void tagSelected();
    void untagSelected();

private:
    TestInterfacePtr m_test;
};

#endif // ITEMTAGSTESTS_H

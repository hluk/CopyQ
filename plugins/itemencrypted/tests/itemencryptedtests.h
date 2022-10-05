// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMENCRYPTEDTESTS_H
#define ITEMENCRYPTEDTESTS_H

#include "tests/testinterface.h"

#include <QObject>

class ItemEncryptedTests final : public QObject
{
    Q_OBJECT
public:
    explicit ItemEncryptedTests(const TestInterfacePtr &test, QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void encryptDecryptData();

    void encryptDecryptItems();

private:
    bool isGpgInstalled() const;

    TestInterfacePtr m_test;
};

#endif // ITEMENCRYPTEDTESTS_H

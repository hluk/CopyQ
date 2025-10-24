// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "tests/testinterface.h"

#include <QObject>

class ItemImageTests final : public QObject
{
    Q_OBJECT
public:
    explicit ItemImageTests(const TestInterfacePtr &test, QObject *parent = nullptr);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void supportedFormats();
    void savePng();
    void saveBmp();
    void saveGif();
    void saveWebp();

private:
    TestInterfacePtr m_test;
};

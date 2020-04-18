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

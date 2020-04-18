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

#include "itemfakevimtests.h"

#include "tests/test_utils.h"

#include <QDir>

#ifdef Q_OS_DARWIN
#define FAKEVIM_CTRL "META"
#else
#define FAKEVIM_CTRL "CTRL"
#endif

ItemFakeVimTests::ItemFakeVimTests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

QString ItemFakeVimTests::fileNameToSource()
{
    return QDir::tempPath() + "/itemfakevim.rc";
}

void ItemFakeVimTests::initTestCase()
{
    TEST(m_test->initTestCase());
}

void ItemFakeVimTests::cleanupTestCase()
{
    TEST(m_test->cleanupTestCase());
}

void ItemFakeVimTests::init()
{
    TEST(m_test->init());

    // Don't use default external editor.
    RUN("config" << "editor" << "", "\n");
}

void ItemFakeVimTests::cleanup()
{
    TEST( m_test->cleanup() );
}

void ItemFakeVimTests::createItem()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;

    RUN(args << "size", "0\n");

    RUN(args << "edit", "");
    RUN(args << "keys" << ":iABC" << "ENTER" << ":DEF"
        << "ESC" << "::wq" << "ENTER", "");

    RUN(args << "read" << "0", "ABC\nDEF");

    RUN(args << "keys" << "F2" << ":GccXYZ" << "ESC" << "::w" << "ENTER", "");
    RUN(args << "read" << "0", "ABC\nXYZ");
    RUN(args << "keys" << ":p:wq" << "ENTER", "");
    RUN(args << "read" << "0", "ABC\nXYZ\nDEF");
}

void ItemFakeVimTests::blockSelection()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;

    RUN(args << "edit", "");
    RUN(args << "keys"
        << ":iABC" << "ENTER" << ":DEF" << "ENTER" << ":GHI" << "ESC" << "::wq" << "ENTER", "");
    RUN(args << "read" << "0", "ABC\nDEF\nGHI");

    RUN(args << "edit" << "0", "");
    RUN(args << "keys"
        << ":ggl" << FAKEVIM_CTRL "+V" << ":jjs_" << "ESC" << "::wq" << "ENTER", "");
    RUN(args << "read" << "0", "A_C\nD_F\nG_I");
}

void ItemFakeVimTests::search()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;

    RUN(args << "edit", "");
    RUN(args << "keys"
        << ":iABC" << "ENTER" << ":DEF" << "ENTER" << ":GHI" << "ESC" << "::wq" << "ENTER", "");
    RUN(args << "read" << "0", "ABC\nDEF\nGHI");

    RUN(args << "edit" << "0", "");
    RUN(args << "keys"
        << ":gg/[EH]" << "ENTER" << ":r_nr_" << "F2", "");
    RUN(args << "read" << "0", "ABC\nD_F\nG_I");
}

void ItemFakeVimTests::incDecNumbers()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;

    RUN(args << "add" << " 0", "");
    RUN(args << "edit" << "0", "");
    RUN(args << "keys" << FAKEVIM_CTRL "+a" << "F2", "");
    RUN(args << "read" << "0", " 1");

    RUN(args << "add" << " -1", "");
    RUN(args << "edit" << "0", "");
    RUN(args << "keys" << FAKEVIM_CTRL "+a" << "F2", "");
    RUN(args << "read" << "0", " 0");

    RUN(args << "add" << " 0xff", "");
    RUN(args << "edit" << "0", "");
    RUN(args << "keys" << FAKEVIM_CTRL "+a" << "F2", "");
    RUN(args << "read" << "0", " 0x100");

    RUN(args << "add" << "9 9 9", "");
    RUN(args << "edit" << "0", "");
    RUN(args << "keys" << "l" << FAKEVIM_CTRL "+a" << "l" << FAKEVIM_CTRL "+a" << "F2", "");
    RUN(args << "read" << "0", "9 10 10");

    RUN(args << "add" << "-1 -1 -1", "");
    RUN(args << "edit" << "0", "");
    RUN(args << "keys" << ":ll" << "2" << FAKEVIM_CTRL "+a" << "l" << "3" << FAKEVIM_CTRL "+a" << "F2", "");
    RUN(args << "read" << "0", "-1 1 2");

    RUN(args << "add" << "1 1 1", "");
    RUN(args << "edit" << "0", "");
    RUN(args << "keys" << "l" << "2" << FAKEVIM_CTRL "+x" << "l" << "3" << FAKEVIM_CTRL "+x" << "F2", "");
    RUN(args << "read" << "0", "1 -1 -2");
}

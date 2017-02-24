/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

    SKIP("Command :w saves item and the editor widget is destroyed because data changed.");
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
        << ":ggl" << "CTRL+V" << ":jjs_" << "ESC" << "::wq" << "ENTER", "");
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

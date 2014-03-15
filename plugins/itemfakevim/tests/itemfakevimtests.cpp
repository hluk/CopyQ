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

#include <QDir>
#include <QTest>

#define RUN(arguments, stdoutExpected) \
    TEST( m_test->runClient(arguments, stdoutExpected) );

namespace {

typedef QStringList Args;

QString testTab(int i)
{
    return "ITEMFAKEVIM_TEST_&" + QString::number(i);
}

} // namespace

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
    TEST(m_test->init());
    cleanup();
}

void ItemFakeVimTests::cleanupTestCase()
{
    TEST(m_test->stopServer());
}

void ItemFakeVimTests::init()
{
    TEST(m_test->init());

    // Don't use default external editor.
    RUN(Args() << "config" << "editor" << "", "");
}

void ItemFakeVimTests::cleanup()
{
    TEST( m_test->cleanup() );
}

void ItemFakeVimTests::createItem()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;

    RUN(Args(args) << "size", "0\n");

    RUN(Args(args) << "edit", "");
    RUN(Args(args) << "keys" << ":iABC" << "ENTER" << ":DEF"
        << "ESC" << "::wq" << "ENTER", "");

    RUN(Args(args) << "read" << "0", "ABC\nDEF");

    SKIP("Command :w saves item and the editor widget is destroyed because data changed.");
    RUN(Args(args) << "keys" << "F2" << ":GccXYZ" << "ESC" << "::w" << "ENTER", "");
    RUN(Args(args) << "read" << "0", "ABC\nXYZ");
    RUN(Args(args) << "keys" << ":p:wq" << "ENTER", "");
    RUN(Args(args) << "read" << "0", "ABC\nXYZ\nDEF");
}

void ItemFakeVimTests::blockSelection()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;

    RUN(Args(args) << "edit", "");
    RUN(Args(args) << "keys"
        << ":iABC" << "ENTER" << ":DEF" << "ENTER" << ":GHI" << "ESC" << "::wq" << "ENTER", "");
    RUN(Args(args) << "read" << "0", "ABC\nDEF\nGHI");

    RUN(Args(args) << "edit" << "0", "");
    RUN(Args(args) << "keys"
        << ":ggl" << "CTRL+V" << ":jjs_" << "ESC" << "::wq" << "ENTER", "");
    RUN(Args(args) << "read" << "0", "A_C\nD_F\nG_I");
}

void ItemFakeVimTests::search()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;

    RUN(Args(args) << "edit", "");
    RUN(Args(args) << "keys"
        << ":iABC" << "ENTER" << ":DEF" << "ENTER" << ":GHI" << "ESC" << "::wq" << "ENTER", "");
    RUN(Args(args) << "read" << "0", "ABC\nDEF\nGHI");

    RUN(Args(args) << "edit" << "0", "");
    RUN(Args(args) << "keys"
        << ":gg/[EH]" << "ENTER" << ":r_nr_" << "F2", "");
    RUN(Args(args) << "read" << "0", "ABC\nD_F\nG_I");
}

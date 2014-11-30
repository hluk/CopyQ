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

#include "itemtagstests.h"

#include "common/mimetypes.h"

#include <QTest>

#define RUN(arguments, stdoutExpected) \
    TEST( m_test->runClient(arguments, stdoutExpected) );

namespace {

typedef QStringList Args;

const int waitMsSearch = 250;

void waitFor(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QApplication::processEvents(QEventLoop::AllEvents, ms);
}

QString testTag(int i)
{
    return "ITEMTAGS_TEST_TAG_&" + QString::number(i);
}

QString testTab(int i)
{
    return "ITEMTAGS_TEST_&" + QString::number(i);
}

} // namespace

ItemSyncTests::ItemSyncTests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

QStringList ItemSyncTests::testTags()
{
    return QStringList()
            << testTag(1)
            << testTag(2)
            << testTag(3)
            << testTag(4)
            << testTag(5);
}

void ItemSyncTests::initTestCase()
{
    TEST(m_test->init());
    cleanup();
}

void ItemSyncTests::cleanupTestCase()
{
    TEST(m_test->stopServer());
}

void ItemSyncTests::init()
{
    TEST(m_test->init());
}

void ItemSyncTests::cleanup()
{
    TEST( m_test->cleanup() );
}

void ItemSyncTests::userTags()
{
    RUN(Args() << "-e" << "plugins.itemtags.userTags",
        QString(testTags().join("\n") + "\n").toUtf8());
}

void ItemSyncTests::tag()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(Args(args) << "add" << "A" << "B" << "C", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(3)", "");
    RUN(Args(args) << "size", "3\n");

    RUN(Args(args) << "-e" << "plugins.itemtags.tag('x', 0)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 0)", "true\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 0)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 1)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 1)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 2)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 2)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(3)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 3)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 3)", "false\n");
    RUN(Args(args) << "size", "3\n");

    RUN(Args(args) << "-e" << "plugins.itemtags.tag('y', 0, 1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\ny\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 0)", "true\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 0)", "true\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "y\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 1)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 1)", "true\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 2)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 2)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(3)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 3)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 3)", "false\n");
    RUN(Args(args) << "size", "3\n");

    RUN(Args(args) << "-e" << "plugins.itemtags.tag('z', 2, 3, 4)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\ny\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 0)", "true\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 0)", "true\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('z', 0)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "y\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 1)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 1)", "true\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('z', 1)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "z\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 2)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 2)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('z', 2)", "true\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(3)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('x', 3)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('y', 3)", "false\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.hasTag('z', 3)", "false\n");
    RUN(Args(args) << "size", "3\n");
}

void ItemSyncTests::untag()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(Args(args) << "add" << "A" << "B" << "C", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tag('x', 0, 1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tag('y', 1, 2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "x\ny\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "y\n");

    RUN(Args(args) << "-e" << "plugins.itemtags.untag('x', 1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "y\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "y\n");

    RUN(Args(args) << "-e" << "plugins.itemtags.untag('y', 1, 2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "");
}

void ItemSyncTests::clearTags()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(Args(args) << "add" << "A" << "B" << "C", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tag('x', 0, 1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tag('y', 1, 2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "x\ny\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "y\n");

    RUN(Args(args) << "-e" << "plugins.itemtags.clearTags(1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "y\n");

    RUN(Args(args) << "-e" << "plugins.itemtags.tag('a', 1, 2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "a\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "a\ny\n");

    RUN(Args(args) << "-e" << "plugins.itemtags.clearTags(0, 2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "a\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "");
}

void ItemSyncTests::searchTags()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(Args(args) << "add" << "A" << "B" << "C", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tag('x', 0, 1)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tag('y', 1, 2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tag('z', 2)", "");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(1)", "x\ny\n");
    RUN(Args(args) << "-e" << "plugins.itemtags.tags(2)", "y\nz\n");

    RUN(Args(args) << "keys" << "RIGHT", "");
    RUN(Args(args) << "keys" << "x", "");
    waitFor(waitMsSearch);
    RUN(Args(args) << "keys" << "TAB" << "CTRL+A", "");
    RUN(Args(args) << "testselecteditems", "0\n1\n");

    RUN(Args(args) << "keys" << "ESCAPE", "");
    RUN(Args(args) << "keys" << "y", "");
    waitFor(waitMsSearch);
    RUN(Args(args) << "keys" << "TAB" << "CTRL+A", "");
    RUN(Args(args) << "testselecteditems", "1\n2\n");

    RUN(Args(args) << "keys" << "ESCAPE", "");
    RUN(Args(args) << "keys" << "z", "");
    waitFor(waitMsSearch);
    RUN(Args(args) << "keys" << "TAB" << "CTRL+A", "");
    RUN(Args(args) << "testselecteditems", "2\n");
}

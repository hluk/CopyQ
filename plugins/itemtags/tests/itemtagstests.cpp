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

#include "itemtagstests.h"

#include "tests/test_utils.h"

namespace {

QString testTag(int i)
{
    return "TAG_&" + QString::number(i);
}

} // namespace

ItemTagsTests::ItemTagsTests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

QStringList ItemTagsTests::testTags()
{
    return QStringList()
            << testTag(1)
            << testTag(2)
            << testTag(3)
            << testTag(4)
            << testTag(5);
}

void ItemTagsTests::initTestCase()
{
    TEST(m_test->initTestCase());
}

void ItemTagsTests::cleanupTestCase()
{
    TEST(m_test->cleanupTestCase());
}

void ItemTagsTests::init()
{
    TEST(m_test->init());
}

void ItemTagsTests::cleanup()
{
    TEST( m_test->cleanup() );
}

void ItemTagsTests::userTags()
{
    RUN("-e" << "plugins.itemtags.userTags",
        QString(testTags().join("\n") + "\n").toUtf8());
}

void ItemTagsTests::tag()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(args << "add" << "A" << "B" << "C", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(3)", "");
    RUN(args << "size", "3\n");

    RUN(args << "-e" << "plugins.itemtags.tag('x', 0)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 0)", "true\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 0)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 1)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 1)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 2)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 2)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.tags(3)", "");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 3)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 3)", "false\n");
    RUN(args << "size", "3\n");

    RUN(args << "-e" << "plugins.itemtags.tag('y', 0, 1)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\ny\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 0)", "true\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 0)", "true\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "y\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 1)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 1)", "true\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 2)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 2)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.tags(3)", "");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 3)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 3)", "false\n");
    RUN(args << "size", "3\n");

    RUN(args << "-e" << "plugins.itemtags.tag('z', 2, 3, 4)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\ny\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 0)", "true\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 0)", "true\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('z', 0)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "y\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 1)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 1)", "true\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('z', 1)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "z\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 2)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 2)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('z', 2)", "true\n");
    RUN(args << "-e" << "plugins.itemtags.tags(3)", "");
    RUN(args << "-e" << "plugins.itemtags.hasTag('x', 3)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('y', 3)", "false\n");
    RUN(args << "-e" << "plugins.itemtags.hasTag('z', 3)", "false\n");
    RUN(args << "size", "3\n");
}

void ItemTagsTests::untag()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(args << "add" << "A" << "B" << "C", "");
    RUN(args << "-e" << "plugins.itemtags.tag('x', 0, 1)", "");
    RUN(args << "-e" << "plugins.itemtags.tag('y', 1, 2)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "x\ny\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "y\n");

    RUN(args << "-e" << "plugins.itemtags.untag('x', 1)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "y\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "y\n");

    RUN(args << "-e" << "plugins.itemtags.untag('y', 1, 2)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "");
}

void ItemTagsTests::clearTags()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(args << "add" << "A" << "B" << "C", "");
    RUN(args << "-e" << "plugins.itemtags.tag('x', 0, 1)", "");
    RUN(args << "-e" << "plugins.itemtags.tag('y', 1, 2)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "x\ny\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "y\n");

    RUN(args << "-e" << "plugins.itemtags.clearTags(1)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "y\n");

    RUN(args << "-e" << "plugins.itemtags.tag('a', 1, 2)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "x\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "a\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "a\ny\n");

    RUN(args << "-e" << "plugins.itemtags.clearTags(0, 2)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "a\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "");
}

void ItemTagsTests::searchTags()
{
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "");
    RUN(args << "add" << "A" << "B" << "C", "");
    RUN(args << "-e" << "plugins.itemtags.tag('tag1', 0, 1)", "");
    RUN(args << "-e" << "plugins.itemtags.tag('tag2', 1, 2)", "");
    RUN(args << "-e" << "plugins.itemtags.tag('tag3', 2)", "");
    RUN(args << "-e" << "plugins.itemtags.tags(0)", "tag1\n");
    RUN(args << "-e" << "plugins.itemtags.tags(1)", "tag1\ntag2\n");
    RUN(args << "-e" << "plugins.itemtags.tags(2)", "tag2\ntag3\n");

    RUN(args << "keys" << "RIGHT", "");
    RUN(args << "keys" << "t" << "a" << "g" << "1", "");
    RUN(args << "keys" << "TAB" << "CTRL+A", "");
    RUN(args << "testSelected", tab1 + " 0 0 1\n");

    RUN(args << "keys" << "ESCAPE", "");
    RUN(args << "keys" << "t" << "a" << "g" << "2", "");
    RUN(args << "keys" << "TAB" << "CTRL+A", "");
    RUN(args << "testSelected", tab1 + " 1 1 2\n");

    RUN(args << "keys" << "ESCAPE", "");
    RUN(args << "keys" << "t" << "a" << "g" << "3", "");
    RUN(args << "keys" << "TAB" << "CTRL+A", "");
    RUN(args << "testSelected", tab1 + " 2 2\n");
}

void ItemTagsTests::tagSelected()
{
    const auto script = R"(
        setCommands([{
            name: 'Add Tag x',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            cmd: 'copyq: plugins.itemtags.tag("x")'
          },
          {
            name: 'Add Tag y',
            inMenu: true,
            shortcuts: ['Ctrl+F2'],
            cmd: 'copyq: plugins.itemtags.tag("y")'
          }])
        )";
    RUN(script, "");

    RUN("add" << "A" << "B" << "C", "");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("plugins.itemtags.tags(0)", "x\n");

    RUN("selectItems(0,1)", "true\n");
    RUN("keys" << "CTRL+F2", "");
    WAIT_ON_OUTPUT("plugins.itemtags.tags(0)", "x\ny\n");
    RUN("plugins.itemtags.tags(1)", "y\n");
}

void ItemTagsTests::untagSelected()
{
    const auto script = R"(
        setCommands([{
            name: 'Remove Tag x',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            cmd: 'copyq: plugins.itemtags.untag("x")'
        }])
        )";
    RUN(script, "");

    RUN("add" << "A" << "B" << "C", "");
    RUN("plugins.itemtags.tag('x', 0, 2)", "");
    RUN("plugins.itemtags.tag('y', 1, 2)", "");
    RUN("plugins.itemtags.tags(0)", "x\n");

    RUN("selectItems(0,1,2)", "true\n");
    RUN("keys" << "CTRL+F1", "");
    WAIT_ON_OUTPUT("plugins.itemtags.tags(0)", "");
    RUN("plugins.itemtags.tags(1)", "y\n");
    RUN("plugins.itemtags.tags(2)", "y\n");
}

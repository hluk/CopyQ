// SPDX-License-Identifier: GPL-3.0-or-later

#include "itempinnedtests.h"

#include "tests/test_utils.h"

ItemPinnedTests::ItemPinnedTests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

void ItemPinnedTests::initTestCase()
{
    TEST(m_test->initTestCase());
}

void ItemPinnedTests::cleanupTestCase()
{
    TEST(m_test->cleanupTestCase());
}

void ItemPinnedTests::init()
{
    TEST(m_test->init());
}

void ItemPinnedTests::cleanup()
{
    TEST( m_test->cleanup() );
}

void ItemPinnedTests::isPinned()
{
    RUN("add" << "b" << "a", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "false\n");
}

void ItemPinnedTests::pin()
{
    RUN("add" << "b" << "a", "");
    RUN("-e" << "plugins.itempinned.pin(1)", "");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "false\n");

    RUN("-e" << "plugins.itempinned.pin(0)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "true\n");
}

void ItemPinnedTests::pinMultiple()
{
    RUN("add" << "d" << "c" << "b" << "a", "");
    RUN("-e" << "plugins.itempinned.pin(1, 2)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(2)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(3)", "false\n");

    RUN("-e" << "plugins.itempinned.pin(1, 2 ,3)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(2)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(3)", "true\n");
}

void ItemPinnedTests::unpin()
{
    RUN("add" << "b" << "a", "");
    RUN("-e" << "plugins.itempinned.pin(0, 1)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "true\n");

    RUN("-e" << "plugins.itempinned.unpin(0)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "true\n");

    RUN("-e" << "plugins.itempinned.unpin(1)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "false\n");
}

void ItemPinnedTests::unpinMultiple()
{
    RUN("add" << "d" << "c" << "b" << "a", "");
    RUN("-e" << "plugins.itempinned.pin(0, 1, 2, 3)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(2)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(3)", "true\n");

    RUN("-e" << "plugins.itempinned.unpin(1, 2)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(2)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(3)", "true\n");

    RUN("-e" << "plugins.itempinned.unpin(2, 3)", "");
    RUN("-e" << "plugins.itempinned.isPinned(0)", "true\n");
    RUN("-e" << "plugins.itempinned.isPinned(1)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(2)", "false\n");
    RUN("-e" << "plugins.itempinned.isPinned(3)", "false\n");
}

void ItemPinnedTests::removePinnedThrows()
{
    RUN("add" << "b" << "a", "");
    RUN("-e" << "plugins.itempinned.pin(0, 1)", "");

    RUN_EXPECT_ERROR("remove" << "0" << "1", CommandException);
    RUN("separator" << " " << "read" << "0" << "1", "a b");
}

void ItemPinnedTests::pinToRow()
{
    const auto read = Args() << "separator" << " " << "read";

    RUN("add" << "a", "");
    RUN("-e" << "plugins.itempinned.pin(0)", "");
    RUN("add" << "b", "");
    RUN(read << "0" << "1", "a b");

    RUN("add" << "c", "");
    RUN(read << "0" << "1" << "2", "a c b");

    RUN("-e" << "plugins.itempinned.pin(1)", "");
    RUN("add" << "d", "");
    RUN(read << "0" << "1" << "2" << "3", "a c d b");

    RUN("-e" << "plugins.itempinned.pin(2)", "");
    RUN("-e" << "plugins.itempinned.unpin(1); remove(1)", "");
    RUN(read << "0" << "1" << "2", "a b d");
}

void ItemPinnedTests::fullTab()
{
    RUN("config" << "maxitems" << "3", "3\n");
    RUN("add" << "c" << "b" << "a", "");
    RUN("-e" << "plugins.itempinned.pin(0,1,2)", "");

    // Tab is full and no items can be removed.
    RUN_EXPECT_ERROR("add" << "X", CommandException);
    RUN_EXPECT_ERROR("write" << "1" << "text/plain" << "X", CommandException);
    RUN("separator" << " " << "read" << "0" << "1" << "2", "a b c");
    RUN("size", "3\n");
}

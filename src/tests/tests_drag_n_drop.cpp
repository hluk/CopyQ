// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

#define DRAG(DRAG_PARENT, SOURCE, TARGET) do { \
    KEYS("mouse|PRESS|" SOURCE); \
    KEYS("mouse|DRAG|" SOURCE); \
    KEYS("isDraggingFrom|" DRAG_PARENT); \
    KEYS("mouse|RELEASE|" TARGET); \
} while(false)

#define ITEM(TEXT) "item|text=" TEXT

#define TAB(TEXT) "tab_tree_item|text=" TEXT

void Tests::dragNDropItemOrder()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "show_simple_items" << "true", "true\n");

    RUN("add" << "ITEM3" << "ITEM2" << "ITEM1", "");
    RUN("read(0,1,2,3)", "ITEM1\nITEM2\nITEM3\n");

    DRAG("ClipboardBrowser", ITEM("ITEM1"), ITEM("ITEM1"));
    RUN("read(0,1,2,3)", "ITEM1\nITEM2\nITEM3\n");

    DRAG("ClipboardBrowser", ITEM("ITEM1"), ITEM("ITEM2"));
    RUN("read(0,1,2,3)", "ITEM1\nITEM2\nITEM3\n");

    DRAG("ClipboardBrowser", ITEM("ITEM1"), ITEM("ITEM3"));
    RUN("read(0,1,2,3)", "ITEM2\nITEM1\nITEM3\n");
}

void Tests::dragNDropItemToTabTree()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "tab_tree" << "true", "true\n");
    RUN("config" << "show_simple_items" << "true", "true\n");
    RUN("config('tabs', ['TAB1','TAB2'])", "TAB1\nTAB2\n");

    RUN("tab" << "TAB1" << "add" << "ITEM0", "");
    RUN("tab" << "TAB2" << "add" << "ITEM3" << "ITEM2" << "ITEM1", "");
    RUN("tab" << "TAB2" << "selectItems" << "1" << "2", "true\n");
    RUN("setCurrentTab('TAB2')", "");

    DRAG("ClipboardBrowser", ITEM("ITEM2"), TAB("TAB1"));
    RUN("tab" << "TAB1" << "read(0,1,2,3)", "ITEM2\nITEM3\nITEM0\n");
    RUN("tab" << "TAB2" << "read(0,1,2,3)", "ITEM1\n\n\n");
}

void Tests::dragNDropTreeTab()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "tab_tree" << "true", "true\n");
    RUN("config('tabs', ['TAB1','TAB2'])", "TAB1\nTAB2\n");
    WAIT_ON_OUTPUT("tab", "TAB1\nTAB2\nCLIPBOARD\n");

    DRAG("tab_tree", TAB("TAB2"), TAB("TAB1"));
    RUN("tab", "TAB1\nTAB1/TAB2\nCLIPBOARD\n");
}

void Tests::dragNDropTreeTabNested()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "tab_tree" << "true", "true\n");
    RUN("config('tabs', ['a/b/c/d','a/b/c'])", "a/b/c/d\na/b/c\n");
    WAIT_ON_OUTPUT("tab", "a/b/c/d\na/b/c\nCLIPBOARD\n");

    DRAG("tab_tree", TAB("c"), TAB("a"));
    RUN("tab", "a/c\na/c/d\nCLIPBOARD\n");
}

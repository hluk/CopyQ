// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include <QFile>
#include <QRegularExpression>
#include <QSettings>

#include "common/config.h"

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

void Tests::dragNDropTreeTabChildCollision()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "tab_tree" << "true", "true\n");

    // Create tabs where dragging group 'a' under 'b' would make
    // child 'a/x' collide with existing 'b/a/x'.
    RUN("config('tabs', ['a/x','b/a/x'])", "a/x\nb/a/x\n");
    WAIT_ON_OUTPUT("tab", "a/x\nb/a/x\nCLIPBOARD\n");

    // Add distinct data to each tab so we can verify nothing is lost.
    RUN("tab" << "a/x" << "add" << "DATA_AX", "");
    RUN("tab" << "b/a/x" << "add" << "DATA_BAX", "");

    // Drag group 'a' onto 'b' — child 'a/x' would become 'b/a/x'
    // which already exists, so the moved tab must be renamed to
    // avoid collision (e.g. 'b/a (1)/x').
    DRAG("tab_tree", TAB("a"), TAB("b"));

    // Both tabs' data must survive.
    // The original 'b/a/x' must keep its data.
    RUN("tab" << "b/a/x" << "read(0)", "DATA_BAX");

    // The moved tab was renamed to avoid collision;
    // verify it exists under a unique name and still has its data.
    QByteArray tabsOutput;
    TEST( m_test->getClientOutput(Args() << "tab", &tabsOutput) );
    const auto tabNames = QString::fromUtf8(tabsOutput).split('\n');

    // Find the renamed tab (not 'b/a/x', not 'CLIPBOARD', not empty).
    bool foundMovedTab = false;
    for (const auto &tabName : tabNames) {
        if (tabName.isEmpty() || tabName == "b/a/x" || tabName == "CLIPBOARD")
            continue;
        // The renamed tab should be under 'b/' and contain the original data.
        QVERIFY2(tabName.startsWith("b/"),
            qPrintable("Moved tab should be under 'b/' but is: " + tabName));
        RUN("tab" << tabName << "read(0)", "DATA_AX");
        foundMovedTab = true;
    }
    QVERIFY2(foundMovedTab, "Moved tab 'a/x' should exist under a unique name after collision");
}

void Tests::dragNDropTreeTabPartialRenameFailure()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "tab_tree" << "true", "true\n");

    // Two child tabs under 'a' plus an unrelated tab under 'b'
    // so group 'b' exists in the tree.
    RUN("config('tabs', ['a/x','a/y','b/z'])", "a/x\na/y\nb/z\n");
    WAIT_ON_OUTPUT("tab", "a/x\na/y\nb/z\nCLIPBOARD\n");

    // Add data to each tab.
    RUN("tab" << "a/x" << "add" << "DATA_AX", "");
    RUN("tab" << "a/y" << "add" << "DATA_AY", "");
    RUN("tab" << "b/z" << "add" << "DATA_BZ", "");

    // Plant a blocker file at the destination path for 'b/a/y'.
    // QFile::copy() will refuse to overwrite the existing file, making
    // moveItems() fail for that one tab while 'a/x' -> 'b/a/x' succeeds.
    const QString blockerPath = getConfigurationFilePath("_tab_")
        + QString::fromUtf8(QByteArray("b/a/y").toBase64()).replace('/', '-')
        + QStringLiteral(".dat");
    QVERIFY(QFile(blockerPath).open(QIODevice::WriteOnly));

    // The ERROR log from moveItems() and the rollback warning are expected.
    m_test->ignoreErrors(QRegularExpression(
        "Failed to move.*Destination file exists|Rolling back tab renames"));

    // Drag group 'a' onto 'b'.
    // No tree-level group collision (b has no child 'a'), so renameToUnique
    // does NOT fire.  moveItems will fail for 'a/y' \u2192 'b/a/y' because the
    // destination .dat file already exists.
    DRAG("tab_tree", TAB("a"), TAB("b"));
    RUN("tab", "a/x\na/y\nb/z\nCLIPBOARD\n");

    // With all-or-nothing rollback, the entire group move is reverted.
    // Both 'a/x' and 'a/y' remain at their original paths with data intact.

    // Remove the blocker file before restart.
    QVERIFY(QFile::remove(blockerPath));

    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );
    RUN("show", "");

    // All tabs must be at their original names with data intact.
    RUN("tab" << "a/x" << "read(0)", "DATA_AX");
    RUN("tab" << "a/y" << "read(0)", "DATA_AY");
    RUN("tab" << "b/z" << "read(0)", "DATA_BZ");

    RUN("tab", "a/x\na/y\nb/z\nCLIPBOARD\n");
    RUN("config('tabs')", "a/x\na/y\nb/z\nCLIPBOARD\n");
}

void Tests::dragNDropTreeTabKeepsCollapsedState()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_DRAG_AND_DROP");

    RUN("config" << "tab_tree" << "true", "true\n");
    RUN("config('tabs', ['a/b','a/c','x'])", "a/b\na/c\nx\n");
    WAIT_ON_OUTPUT("tab", "a/b\na/c\nx\nCLIPBOARD\n");

    // Stop the server, inject collapsed state for group 'a', and restart.
    const QString tabsIniPath = getConfigurationFilePath("_tabs.ini");
    TEST( m_test->stopServer() );
    {
        QSettings settings(tabsIniPath, QSettings::IniFormat);
        settings.setValue("TabWidget/collapsed_tabs", QStringList{"a"});
        settings.sync();
    }
    TEST( m_test->startServer() );
    RUN("show", "");

    // Drag tab 'x' onto group 'a' \u2014 it becomes 'a/x'.
    // Group 'a' was collapsed; it must stay collapsed after the move.
    DRAG("tab_tree", TAB("x"), TAB("a"));
    RUN("tab", "a/b\na/c\na/x\nCLIPBOARD\n");

    // Verify collapsed state is preserved in the config file.
    {
        QSettings settings(tabsIniPath, QSettings::IniFormat);
        const QStringList collapsed = settings.value("TabWidget/collapsed_tabs").toStringList();
        QVERIFY2(collapsed.contains("a"),
            qPrintable("Group 'a' should remain collapsed but collapsed_tabs = ["
                       + collapsed.join(", ") + "]"));
    }
}

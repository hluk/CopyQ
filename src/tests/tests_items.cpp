// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/mimetypes.h"
#include "common/commandstatus.h"

void Tests::configMaxitems()
{
    RUN("config" << "maxitems" << "3", "3\n");
    RUN("add" << "A" << "B" << "C", "");
    RUN("add" << "D", "");
    RUN("separator" << " " << "read" << "0" << "1" << "2", "D C B");
    RUN("size", "3\n");

    RUN("add" << "E" << "F", "");
    RUN("separator" << " " << "read" << "0" << "1" << "2", "F E D");
    RUN("size", "3\n");

    RUN("config" << "maxitems" << "2", "2\n");
    RUN("separator" << " " << "read" << "0" << "1", "F E");
    RUN("size", "2\n");

    // Adding too many items fails.
    RUN_EXPECT_ERROR("add" << "1" << "2" << "3", CommandException);
    RUN("separator" << " " << "read" << "0" << "1", "F E");
    RUN("size", "2\n");

    // Single item in tabs.
    RUN("config" << "maxitems" << "1", "1\n");
    RUN("separator" << " " << "read" << "0", "F");
    RUN("size", "1\n");

    RUN("add" << "G", "");
    RUN("separator" << " " << "read" << "0", "G");
    RUN("size", "1\n");

    RUN("write" << "1" << "text/plain" << "H", "");
    RUN("separator" << " " << "read" << "0", "H");
    RUN("size", "1\n");

    // No items in tabs.
    RUN("config" << "maxitems" << "0", "0\n");
    RUN("size", "0\n");

    RUN_EXPECT_ERROR("add" << "1", CommandException);
    RUN_EXPECT_ERROR("write" << "1", CommandException);
    RUN("size", "0\n");

    // Invalid value.
    RUN("config" << "maxitems" << "-99", "0\n");
    RUN("size", "0\n");
}

void Tests::keysAndFocusing()
{
    RUN("disable", "");
    RUN("keys" << clipboardBrowserId << "CTRL+T", "");
    WAIT_ON_OUTPUT("currentWindowTitle", appWindowTitle("New Tab"));

    RUN("keys" << tabDialogLineEditId << "ESC", "");
    WAIT_ON_OUTPUT("currentWindowTitle", appWindowTitle("*Clipboard Storing Disabled*"));

    RUN("enable", "");
}

void Tests::selectItems()
{
    const auto tab = QString(clipboardTabName);
    RUN("add" << "C" << "B" << "A", "");

    RUN("keys" << "RIGHT" << "SHIFT+DOWN" << "SHIFT+DOWN", "");
    RUN("testSelected", tab + " 2 0 1 2\n");

    RUN("keys" << "SHIFT+UP", "");
    RUN("testSelected", tab + " 1 0 1\n");

    RUN("keys" << "END", "");
    RUN("testSelected", tab + " 2 2\n");

    RUN("keys" << "SHIFT+UP", "");
    RUN("testSelected", tab + " 1 1 2\n");

    RUN("keys" << "CTRL+A", "");
    RUN("testSelected", tab + " 1 0 1 2\n");

    // CTRL+SPACE toggles current item selection
    RUN("add" << "D", "");
    RUN("keys" << "PGUP" << "CTRL+SHIFT+DOWN" << "CTRL+SHIFT+DOWN", "");
    RUN("testSelected", tab + " 2 0\n");
    RUN("keys" << "CTRL+SPACE", "");
    RUN("testSelected", tab + " 2 0 2\n");
    RUN("keys" << "SHIFT+DOWN", "");
    RUN("testSelected", tab + " 3 0 2 3\n");
    RUN("keys" << "CTRL+SPACE", "");
    RUN("testSelected", tab + " 3 0 2\n");
}

void Tests::moveItems()
{
    const auto tab = QString(clipboardTabName);
    const auto args = Args() << "separator" << " ";
    RUN(args << "add" << "C" << "B" << "A", "");

    // move item one down
    RUN(args << "keys" << "RIGHT" << "CTRL+DOWN", "");
    RUN(args << "read" << "0" << "1" << "2", "B A C");
    RUN(args << "testSelected", tab + " 1 1\n");

    // move items to top
    RUN(args << "keys" << "SHIFT+DOWN" << "CTRL+HOME", "");
    RUN(args << "read" << "0" << "1" << "2", "A C B");
    RUN(args << "testSelected", tab + " 1 0 1\n");
}

void Tests::deleteItems()
{
    const auto tab = QString(clipboardTabName);
    const auto args = Args() << "separator" << ",";
    RUN(args << "add" << "C" << "B" << "A", "");

    // delete first item
    RUN(args << "keys" << "RIGHT" << m_test->shortcutToRemove(), "");
    RUN(args << "read" << "0" << "1" << "2", "B,C,");
    RUN(args << "testSelected", tab + " 0 0\n");

    // select all and delete
    RUN(args << "keys" << "CTRL+A" << m_test->shortcutToRemove(), "");
    RUN(args << "size", "0\n");
}

void Tests::searchItems()
{
    RUN("add" << "a" << "b" << "c", "");
    RUN("keys" << ":b" << "TAB", "");
    RUN("testSelected", QString(clipboardTabName) + " 1 1\n");
}

void Tests::searchItemsAndSelect()
{
    RUN("add" << "xx1" << "a" << "xx2" << "c" << "xx3" << "d", "");
    RUN("keys" << ":xx" << filterEditId, "");
    RUN("testSelected", QString(clipboardTabName) + " 1 1\n");

    RUN("keys" << filterEditId << "DOWN" << filterEditId, "");
    RUN("testSelected", QString(clipboardTabName) + " 3 3\n");

    RUN("keys" << filterEditId << "DOWN" << filterEditId, "");
    RUN("testSelected", QString(clipboardTabName) + " 5 5\n");

    RUN("keys" << filterEditId << "TAB" << clipboardBrowserId, "");
}

void Tests::searchItemsAndCopy()
{
    RUN("add" << "TEST_ITEM", "");
    RUN("keys" << ":test" << "CTRL+C" << filterEditId, "");
    WAIT_FOR_CLIPBOARD("TEST_ITEM");
}

void Tests::searchRowNumber()
{
    RUN("add" << "d2" << "c" << "b2" << "a", "");

    RUN("keys" << ":2" << "TAB", "");
    RUN("testSelected", QString(clipboardTabName) + " 1 1\n");
    RUN("keys" << "CTRL+A", "");
    RUN("testSelected", QString(clipboardTabName) + " 1 1 3\n");

    RUN("config" << "row_index_from_one" << "false", "false\n");
    RUN("keys" << ":2" << "TAB", "");
    RUN("testSelected", QString(clipboardTabName) + " 2 2\n");
    RUN("keys" << "CTRL+A", "");
    RUN("testSelected", QString(clipboardTabName) + " 2 1 2 3\n");
}

void Tests::searchAccented()
{
    RUN("add" << "a" << "väčšina" << "a", "");
    RUN("filter" << "vacsina", "");
    WAIT_ON_OUTPUT("testSelected", QByteArray(clipboardTabName) + " 1 1\n");
}

void Tests::copyItems()
{
    const auto tab = QString(clipboardTabName);
    RUN("add" << "C" << "B" << "A", "");

    // Select and copy all items.
    RUN("keys" << "CTRL+A" << keyNameFor(QKeySequence::Copy), "");

    // This seems to be required on Windows.
    WAIT_ON_OUTPUT("clipboard", "A\nB\nC");

    // Paste all items.
    RUN("keys" << keyNameFor(QKeySequence::Paste), "");
    RUN("separator" << " " << "read" << "0" << "1" << "2" << "3" << "4" << "5", "A B C A B C");
    RUN("size", "6\n");
}

void Tests::selectAndCopyOrder()
{
    const auto tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";
    RUN(args << "add" << "D" << "C" << "B" << "A", "");
    RUN("setCurrentTab" << tab, "");

    RUN("keys" << "END" << "SHIFT+UP" << "SHIFT+UP" << "SHIFT+UP", "");
    RUN(args << "testSelected", tab + " 0 0 1 2 3\n");

    RUN("keys" << keyNameFor(QKeySequence::Copy), "");
    WAIT_ON_OUTPUT("clipboard", "D\nC\nB\nA");
}

void Tests::sortAndReverse()
{
    const auto tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";
    RUN(args << "add" << "D" << "A" << "C" << "B", "");
    RUN("setCurrentTab" << tab, "");

    RUN("keys" << "CTRL+A", "");
    RUN(args << "testSelected", tab + " 0 0 1 2 3\n");

    RUN("keys" << "CTRL+SHIFT+S", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "A B C D ");
    RUN(args << "testSelected", tab + " 1 0 1 2 3\n");
    RUN("keys" << keyNameFor(QKeySequence::Copy), "");
    WAIT_ON_OUTPUT("clipboard", "A\nB\nC\nD");

    RUN("keys" << "CTRL+SHIFT+R", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "D C B A ");
    RUN(args << "testSelected", tab + " 2 0 1 2 3\n");
    RUN("keys" << keyNameFor(QKeySequence::Copy), "");
    WAIT_ON_OUTPUT("clipboard", "D\nC\nB\nA");

    RUN("keys" << "CTRL+SHIFT+R", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "A B C D ");
    RUN(args << "testSelected", tab + " 1 0 1 2 3\n");
}

void Tests::editItems()
{
    RUN("config" << "edit_ctrl_return" << "true", "true\n");

    RUN("add" << "Line 4" << "Line 1", "");

    RUN("keys"
        << clipboardBrowserId << "F2"
        << editorId << "END" << "ENTER" << ":Line 2" << "F2", "");
    RUN("read" << "0", "Line 1\nLine 2");

    RUN("keys"
        << clipboardBrowserId << "DOWN" << "F2"
        << editorId << "HOME" << ":Line 3" << "ENTER" << "F2", "");
    RUN("read" << "1", "Line 3\nLine 4");
    RUN("read" << "0", "Line 1\nLine 2");

    // Edit multiple items
    RUN("keys"
        << clipboardBrowserId << "SHIFT+UP" << "F2"
        << editorId << "END" << "ENTER" << ":Line 5" << "F2", "");
    RUN("read" << "0", "Line 3\nLine 4\nLine 1\nLine 2\nLine 5");
    RUN("read" << "1", "Line 1\nLine 2");
    RUN("read" << "2", "Line 3\nLine 4");
}

void Tests::createNewItem()
{
    RUN("config" << "edit_ctrl_return" << "true", "true\n");

    RUN("keys" << "CTRL+N" << editorId << ":Line 1" << "ENTER" << ":Line 2" << "F2", "");
    RUN("read" << "0", "Line 1\nLine 2");

    RUN("keys" << "CTRL+N" << editorId << ":Line 3" << "ENTER" << ":Line 4" << "F2", "");
    RUN("read" << "0", "Line 3\nLine 4");
}

void Tests::editNotes()
{
    const auto script = R"(
        add(
          {
            [mimeText]: 'B',
            // https://stackoverflow.com/a/13139830
            [mimeIcon]: frombase64("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7"),
          },
          {
            [mimeText]: 'A',
            [mimeIcon]: String.fromCharCode(0xf188),
          }
        )
    )";
    RUN(script, "");

    RUN("config" << "editor" << "", "\n");
    RUN("keys" << "SHIFT+F2" << ":A Note" << "F2", "");
    RUN("read" << mimeText << "0" << mimeItemNotes << "0" << "F2", "A\nA Note");
    RUN("read" << mimeText << "1" << mimeItemNotes << "1" << "F2", "B\n");

    RUN("keys" << "DOWN", "");

    RUN("keys" << "SHIFT+F2" << ":B Note" << "F2", "");
    RUN("read" << mimeText << "1" << mimeItemNotes << "1" << "F2", "B\nB Note");
    RUN("read" << mimeText << "0" << mimeItemNotes << "0" << "F2", "A\nA Note");
}

// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/commandstatus.h"
#include "common/mimetypes.h"
#include "common/sleeptimer.h"
#include "common/version.h"

#include <QProcess>

namespace {

QString copyqUserAgent()
{
    return QStringLiteral("CopyQ/%1").arg(versionString);
}

bool waitWhileFileExists(const QFile &file)
{
    SleepTimer t(2000);
    while (file.exists() && t.sleep()) {}
    return !file.exists();
}

QVariantMap secretData(const QByteArray &text)
{
#ifdef Q_OS_WIN
    const QString format("application/x-qt-windows-mime;value=\"Clipboard Viewer Ignore\"");
    const QByteArray value("");
#elif defined(Q_OS_MACOS)
    const QString format("application/x-nspasteboard-concealed-type");
    const QByteArray value("secret");
#elif defined(Q_OS_UNIX)
    const QString format("x-kde-passwordManagerHint");
    const QByteArray value("secret");
#endif

    return QVariantMap{
        {format, value},
        {mimeText, text},
    };
}

} // namespace


void Tests::pipingCommands()
{
    const auto tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << ",";

    RUN(args << "action"
        << "copyq print HELLO | copyq print(str(input()).toLowerCase())", "");
    WAIT_ON_OUTPUT(args << "read" << "0" << "1", "hello,");

    RUN(args << "action"
        << "copyq print TEST"
        " | copyq 'print(str(input()) + 1)'"
        " | copyq 'print(str(input()) + 2)'"
        " | copyq 'print(str(input()) + 3)'"
        , "");
    WAIT_ON_OUTPUT(args << "read" << "0" << "1", "TEST123,hello");
}

void Tests::copyPasteCommands()
{
    const QByteArray commands =
            "[Commands]\n"
            "1\\Name=Test 1\n"
            "2\\Name=Test 2\n"
            "size=2";

    KEYS(clipboardBrowserId << "F6");
    TEST( m_test->setClipboard(commands) );
    KEYS(commandDialogListId << keyNameFor(QKeySequence::Paste));

    TEST( m_test->setClipboard(QByteArray()) );
    KEYS(commandDialogListId << keyNameFor(QKeySequence::Copy));
    WAIT_FOR_CLIPBOARD(commands);

    KEYS(commandDialogListId << "Enter" << clipboardBrowserId);
    RUN("commands().length", "2\n");
}

void Tests::toggleClipboardMonitoring()
{
    const QByteArray data1 = generateData();
    TEST( m_test->setClipboard(data1) );
    RUN("clipboard", data1);
    WAIT_ON_OUTPUT("read" << "0", data1);

    RUN("disable", "");
    RUN("monitoring", "false\n");
    WAIT_ON_OUTPUT("isClipboardMonitorRunning", "false\n");

    const QByteArray data2 = generateData();
    TEST( m_test->setClipboard(data2) );
    RUN("clipboard", data2);
    WAIT_ON_OUTPUT("read" << "0", data1);

    RUN("enable", "");
    RUN("monitoring", "true\n");
    WAIT_ON_OUTPUT("isClipboardMonitorRunning", "true\n");

    const QByteArray data3 = generateData();
    TEST( m_test->setClipboard(data3) );
    RUN("clipboard", data3);
    WAIT_ON_OUTPUT("read" << "0", data3);
}

void Tests::clipboardToItem()
{
    TEST( m_test->setClipboard("TEXT1") );
    RUN("clipboard", "TEXT1");
    WAIT_ON_OUTPUT("read" << "0", "TEXT1");
    RUN("read" << "?" << "0", "text/plain\n");

    TEST( m_test->setClipboard("DATA1", "DATA") );
    WAIT_ON_OUTPUT("clipboard" << "DATA", "DATA1");

    // Unicode test.
    const auto test = QString::fromUtf8(QByteArray("Zkouška s různými českými znaky!"));
    const auto bytes = test.toUtf8();
    TEST( m_test->setClipboard(bytes) );
    RUN("clipboard", bytes);
    WAIT_ON_OUTPUT("read" << "0", bytes);
}

void Tests::itemToClipboard()
{
    RUN("add" << "TESTING2" << "TESTING1", "");
    RUN("read" << "0" << "1", "TESTING1\nTESTING2");

    RUN("select" << "0", "");

    WAIT_FOR_CLIPBOARD("TESTING1");
    RUN("clipboard", "TESTING1");

    // select second item and move to top
    RUN("config" << "move" << "true", "true\n");
    RUN("select" << "1", "");
    RUN("read" << "0" << "1", "TESTING2\nTESTING1");

    WAIT_FOR_CLIPBOARD("TESTING2");
    RUN("clipboard", "TESTING2");

    // select without moving
    RUN("config" << "move" << "0", "false\n");
    RUN("select" << "1", "");
    RUN("read" << "0" << "1", "TESTING2\nTESTING1");

    WAIT_FOR_CLIPBOARD("TESTING1");
    RUN("clipboard", "TESTING1");
}

void Tests::tabAdd()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    QVERIFY( !hasTab(tab) );
    RUN(args, "");
    RUN(args << "size", "0\n");
    RUN(args << "add" << "ghi" << "def" << "abc", "");
    QVERIFY( hasTab(tab) );

    // Restart server.
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );

    QVERIFY( hasTab(tab) );

    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "abc def ghi");
}

void Tests::tabRemove()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    RUN(args << "add" << "", "");
    QVERIFY( hasTab(tab) );
    RUN(Args() << "removetab" << tab, "");
    QVERIFY( !hasTab(tab) );

    RUN_EXPECT_ERROR("removetab" << tab, CommandException);
}

void Tests::tabIcon()
{
    const QString tab0 = testTab(1);
    const QString tab = testTab(2);
    const QString icon = ":/images/icon";

    RUN("tab" << tab << "add" << "", "");
    RUN("tabIcon" << tab, "\n");
    RUN("tabicon" << tab0, "\n");
    RUN("tabicon" << tab << icon, "");
    RUN("tabIcon" << tab, icon + "\n");
    RUN("tabicon" << tab0, "\n");
    RUN("tabIcon" << tab << "", "");
    RUN("tabicon" << tab, "\n");
}

void Tests::action()
{
    const Args args = Args("tab") << testTab(1);
    const Args argsAction = Args(args) << "action";
    const QString action = QString::fromLatin1("copyq %1 %2").arg(args.join(" "));

    // action with size
    RUN(argsAction << action.arg("size") << "", "");
    WAIT_ON_OUTPUT(args << "size", "1\n");
    RUN(args << "read" << "0", "0\n");

    // action with size
    RUN(argsAction << action.arg("size") << "", "");
    WAIT_ON_OUTPUT(args << "size", "2\n");
    RUN(args << "read" << "0", "1\n");

    // action with eval print
    RUN(argsAction << action.arg("eval 'print(\"A,B,C\")'") << "", "");
    WAIT_ON_OUTPUT(args << "size", "3\n");
    RUN(args << "read" << "0", "A,B,C");

    // action with read and comma separator for new items
    RUN(argsAction << action.arg("read 0") << ",", "");
    WAIT_ON_OUTPUT(args << "size", "6\n");
    RUN(args << "read" << "0" << "1" << "2", "C\nB\nA");
}

void Tests::renameTab()
{
    const QString tab1 = testTab(1);
    const QString tab2 = testTab(2);

    RUN("tab" << tab1 << "add" << "ghi" << "def" << "abc", "");

    RUN("renametab" << tab1 << tab2, "");
    RUN("tab" << tab2 << "size", "3\n");
    RUN("tab" << tab2 << "read" << "0" << "1" << "2", "abc\ndef\nghi");
    QVERIFY( !hasTab(tab1) );

    // Rename non-existing tab.
    RUN_EXPECT_ERROR("renametab" << tab1 << tab2, CommandException);

    // Rename to same name.
    RUN_EXPECT_ERROR("renametab" << tab2 << tab2, CommandException);

    // Rename to empty name.
    RUN_EXPECT_ERROR("renametab" << tab2 << "", CommandException);

    // Rename to existing tab.
    RUN_EXPECT_ERROR("renametab" << tab2 << clipboardTabName, CommandException);

    QVERIFY( !hasTab(tab1) );
    QVERIFY( hasTab(tab2) );

    RUN("renametab" << tab2 << tab1, "");
    RUN("tab" << tab1 << "read" << "0" << "1" << "2", "abc\ndef\nghi");

    QVERIFY( hasTab(tab1) );
    QVERIFY( !hasTab(tab2) );
}

void Tests::renameClipboardTab()
{
    const QString newClipboardTabName = clipboardTabName + QStringLiteral("2");
    RUN("config" << "tray_tab" << clipboardTabName, clipboardTabName + QStringLiteral("\n"));
    const QString icon = ":/images/icon";
    RUN("tabicon" << clipboardTabName << icon, "");

    RUN("renametab" << clipboardTabName << newClipboardTabName, "");
    RUN("tab", newClipboardTabName + "\n");
    RUN("config" << "clipboard_tab", newClipboardTabName + QStringLiteral("\n"));
    RUN("config" << "tray_tab", newClipboardTabName + QStringLiteral("\n"));
    RUN("tabicon" << newClipboardTabName, icon + QStringLiteral("\n"));

    TEST( m_test->setClipboard("test1") );
    WAIT_ON_OUTPUT("tab" << newClipboardTabName << "read" << "0", "test1");
    RUN("tab", newClipboardTabName + "\n");

    WAIT_ON_OUTPUT("read" << "0", "test1");
    RUN("tab", newClipboardTabName + "\n");
}

void Tests::importExportTab()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    RUN(args << "add" << "ghi" << "def" << "abc", "");

    TemporaryFile tmp;
    RUN(args << "exporttab" << tmp.fileName(), "");

    RUN("removetab" << tab, "");
    QVERIFY( !hasTab(tab) );

    RUN(args << "importtab" << tmp.fileName(), "");
    RUN(args << "read" << "0" << "1" << "2", "abc def ghi");
    RUN(args << "size", "3\n");

    // Export with relative path.
    TemporaryFile tmp2;

    // Change back to original working directory once finished.
    struct CurrentDirectoryGuard {
        CurrentDirectoryGuard() : oldDir(QDir::currentPath()) {}
        ~CurrentDirectoryGuard() { QDir::setCurrent(oldDir); }
        const QString oldDir;
    } currentDirectoryGuard;

    QDir::setCurrent( QDir::cleanPath(tmp2.fileName() + "/..") );

    const QString fileName = QFileInfo( tmp2.fileName() ).fileName();

    RUN(args << "add" << "012", "");
    RUN(args << "exporttab" << fileName, "");

    RUN("removetab" << tab, "");
    QVERIFY( !hasTab(tab) );

    RUN(args << "importtab" << fileName, "");
    RUN(args << "read" << "0" << "1" << "2" << "3", "012 abc def ghi");
}

void Tests::removeAllFoundItems()
{
    auto args = Args("add");
    for (int i = 0; i < 50; ++i) {
        args << QString::fromLatin1("a%1").arg(i);
        args << QString::fromLatin1("b%1").arg(i);
    }

    RUN(args, "");
    RUN("size", "100\n");

    RUN("filter" << "a", "");
    KEYS("CTRL+A" << m_test->shortcutToRemove());

    RUN("size", "50\n");
    RUN("read" << "49" << "48" << "47", "b0\nb1\nb2");
    RUN("read" << "0" << "1" << "2", "b49\nb48\nb47");
}

void Tests::nextPrevious()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");
    RUN("setCurrentTab" << tab, "");

    RUN(args << "next", "");
    WAIT_FOR_CLIPBOARD("B");

    RUN(args << "next", "");
    WAIT_FOR_CLIPBOARD("C");

    RUN(args << "next", "");
    WAIT_FOR_CLIPBOARD("C");

    RUN(args << "previous", "");
    WAIT_FOR_CLIPBOARD("B");

    RUN(args << "previous", "");
    WAIT_FOR_CLIPBOARD("A");

    RUN(args << "previous", "");
    WAIT_FOR_CLIPBOARD("A");
}

void Tests::externalEditor()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;
    const QString editorTab = testTab(2);
    const Args editorArgs = Args("tab") << editorTab;
    const Args editorFileNameArgs = Args(editorArgs) << "read" << "0";
    const Args editorEndArgs = Args(editorArgs) << "remove" << "0";

    // Set editor command which add file name to edit to special editor tab.
    // The command finishes when the special tab is emptied by this test.
    // File to edit is removed by application when the command finished.
    const auto cmd = QString(
        R"(copyq tab "%1" eval "add(arguments[1]); while(length()) sleep(100);" --)"
    ).arg(editorTab) + " %1";
    RUN("config" << "editor" << cmd, cmd + "\n");

    // Set clipboard.
    const QByteArray data1 = generateData();
    TEST( m_test->setClipboard(data1) );
    RUN("clipboard", data1);

#define EDIT(DATA1, DATA2) \
    do { \
        WAIT_ON_OUTPUT(editorArgs << "size", "1\n"); \
        QByteArray out; \
        QByteArray err; \
        run(editorFileNameArgs, &out, &err); \
        QVERIFY2( testStderr(err), err ); \
        QFile file(out); \
        QVERIFY( file.exists() ); \
        QVERIFY( file.open(QIODevice::ReadWrite) ); \
        QVERIFY( file.readAll() == (DATA1) ); \
        file.write(DATA2); \
        file.close(); \
        RUN(editorEndArgs, ""); \
        waitWhileFileExists(file); \
    } while(false)

    // Edit clipboard.
    RUN("edit" << "-1", "");
    const QByteArray data2 = generateData();
    EDIT(data1, data2);

    // Check if clipboard changed.
    WAIT_ON_OUTPUT("read" << "0", data1 + data2);
    WAIT_FOR_CLIPBOARD(data1 + data2);

    // Edit existing item.
    const QString text =
            "Some text to edit,\n"
            "with second line!\n"
            + generateData();
    RUN(args << "add" << text, "");

    // Modify first item.
    RUN(args << "edit" << "0", "");
    const QByteArray data3 = generateData();
    EDIT(text.toUtf8(), data3);

    // Check first item.
    WAIT_ON_OUTPUT(args << "read" << "0", text.toUtf8() + data3);

    // Edit new item.
    RUN(args << "edit", "");
    const QByteArray data4 = generateData();
    EDIT(QByteArray(), data4);

    // Check first item.
    WAIT_ON_OUTPUT(args << "read" << "0", data4);

#undef EDIT
}

void Tests::nextPreviousTab()
{
    const auto tab1 = testTab(1);
    const auto tab2 = testTab(2);
    const auto setTabsCommand =
        QStringLiteral("config('tabs', ['%1','%2'])").arg(tab1, tab2);
    RUN(setTabsCommand, tab1 + "\n" + tab2 + "\n");

    using KeyPair = QPair<QString, QString>;
    const QList<KeyPair> keyPairs = QList<KeyPair>()
            << KeyPair(keyNameFor(QKeySequence::NextChild), keyNameFor(QKeySequence::PreviousChild))
            << KeyPair("RIGHT", "LEFT");

    for (const auto &keyPair : keyPairs) {
        for (const auto &optionValue : {"false", "true"}) {
            RUN("config" << "tab_tree" << optionValue, QString(optionValue) + "\n");

            KEYS(keyPair.first);
            TEST_SELECTED(tab1 + "\n");
            KEYS(keyPair.first);
            TEST_SELECTED(tab2 + "\n");
            KEYS(keyPair.first);

            KEYS(keyPair.second);
            TEST_SELECTED(tab2 + "\n");
            KEYS(keyPair.second);
            TEST_SELECTED(tab1 + "\n");
            KEYS(keyPair.second);
        }
    }
}

void Tests::itemPreview()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "def" << "abc", "");
    RUN("setCurrentTab" << tab1, "");

    RUN("preview", "false\n");
    KEYS(clipboardBrowserId << "F7");
    RUN("preview", "true\n");

    KEYS(clipboardBrowserId << "TAB" << itemPreviewId);
    KEYS(itemPreviewId << "HOME");
    KEYS(itemPreviewId << "RIGHT");
    KEYS(itemPreviewId << "SHIFT+RIGHT");
    KEYS(itemPreviewId << keyNameFor(QKeySequence::Copy));
    WAIT_FOR_CLIPBOARD("b");

    KEYS(itemPreviewId << "F7" << clipboardBrowserId);

    RUN("preview" << "true", "false\n");
    RUN("preview" << "false", "true\n");
    RUN("preview" << "1", "false\n");
    RUN("preview" << "0", "true\n");
    RUN("preview(true)", "false\n");
    RUN("preview(false)", "true\n");
    RUN("preview", "false\n");
}

void Tests::openAndSavePreferences()
{
#ifdef Q_OS_MAC
    SKIP("Can't focus configuration checkboxes on OS X");
#endif

    RUN("config" << "check_clipboard" << "false", "false\n");

    // Open preferences dialog.
    KEYS("Ctrl+P" << configurationDialogId);

    // Focus and set wrap text option.
    // This behavior could differ on some systems and in other languages.
    KEYS(configurationDialogId << "ALT+1");
    // Wait for any checkbox animation or delay
    waitFor(1000);
    KEYS(configurationDialogId << "ENTER" << clipboardBrowserId);
    WAIT_ON_OUTPUT("config" << "check_clipboard", "true\n");
}

void Tests::pasteFromMainWindow()
{
    RUN("config"
        << "activate_closes" << "true"
        << "activate_focuses" << "true"
        << "activate_pastes" << "true"
        ,
        "activate_closes=true\n"
        "activate_focuses=true\n"
        "activate_pastes=true\n"
        );

    RUN("add" << "TEST", "");
    RUN("hide", "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('text')", "TEST\n"); },
        [&]() {
            KEYS("focus::QLineEdit<.*:QDialog");
            RUN("show", "");
            KEYS(clipboardBrowserId << "ENTER");

            WAIT_FOR_CLIPBOARD("TEST");
            waitFor(waitMsPasteClipboard);

            KEYS("focus::QLineEdit<.*:QDialog" << "ENTER");
        }
    );
}

void Tests::pasteNext()
{
    const auto tab1 = testTab(1);
    RUN("setCurrentTab" << tab1, "");
    KEYS(clipboardBrowserId << "CTRL+N" << editorId << ":NEW ");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "test3" << "test2" << "test1", "");
    RUN("tab" << tab2 << "next(); paste(); next()", "");
    waitFor(waitMsPasteClipboard);

    KEYS(editorId);
    WAIT_FOR_CLIPBOARD("test3");
    KEYS(editorId << "F2");
    RUN("tab" << tab1 << "read" << "0", "NEW test2");
}

void Tests::configAutostart()
{
    SKIP_ON_ENV("COPYQ_TESTS_NO_AUTOSTART");
    RUN("config" << "autostart" << "true", "true\n");
    RUN("config" << "autostart", "true\n");
    RUN("config" << "autostart" << "false", "false\n");
    RUN("config" << "autostart", "false\n");
}

void Tests::configPathEnvVariable()
{
    const auto path = QDir::home().absoluteFilePath("copyq-settings");
    const auto environment = QStringList("COPYQ_SETTINGS_PATH=" + path);

    QByteArray out;
    QByteArray err;
    run(Args() << "info" << "config", &out, &err, QByteArray(), environment);
    QVERIFY2( testStderr(err), err );

    const auto expectedOut = path.toUtf8();
    QCOMPARE( out.left(expectedOut.size()), expectedOut );
}

void Tests::itemDataPathEnvVariable()
{
    const auto path = QDir::home().absoluteFilePath("copyq-data");
    const auto environment = QStringList("COPYQ_ITEM_DATA_PATH=" + path);

    QByteArray out;
    QByteArray err;
    run(Args() << "info" << "data", &out, &err, QByteArray(), environment);
    QVERIFY2( testStderr(err), err );

    const auto expectedOut = path.toUtf8();
    QCOMPARE( out.left(expectedOut.size()), expectedOut );
}

void Tests::configTabs()
{
    const QString sep = QStringLiteral("\n");
    RUN("config" << "tabs", clipboardTabName + sep);

    const QString tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "test", "");
    RUN("config" << "tabs", clipboardTabName + sep + tab1 + sep);

    const QString tab2 = testTab(2);
    RUN(QString("config('tabs', ['%1', '%2'])").arg(clipboardTabName, tab2), clipboardTabName + sep + tab2 + sep);
    RUN("config" << "tabs", clipboardTabName + sep + tab2 + sep + tab1 + sep);
    RUN("tab", clipboardTabName + sep + tab2 + sep + tab1 + sep);

    RUN(QString("config('tabs', ['%1', '%2'])").arg(tab1, tab2), tab1 + sep + tab2 + sep);
    RUN("config" << "tabs", tab1 + sep + tab2 + sep + clipboardTabName + sep);
    RUN("tab", tab1 + sep + tab2 + sep + clipboardTabName + sep);
}

void Tests::selectedItems()
{
    const auto tab1 = testTab(1);
    const Args args = Args("tab") << tab1;

    RUN("selectedTab", "CLIPBOARD\n");
    RUN("selectedItems", "");

    RUN(args << "add" << "D" << "C" << "B" << "A", "");
    RUN(args << "setCurrentTab" << tab1 << "selectItems" << "1" << "2", "true\n");
    RUN("selectedTab", tab1 + "\n");
    RUN(args << "selectedItems", "1\n2\n");
    RUN(args << "currentItem", "2\n");

    const auto print = R"(
        tab(selectedTab());
        print([selectedTab(), "c:" + currentItem(), "s:" + selectedItems()]);
        print("\\n")
    )";

    // Selection stays consistent when moving items
    RUN(print << "move(0)" << print, tab1 + ",c:2,s:1,2\n" + tab1 + ",c:1,s:0,1\n");
    RUN(print, tab1 + ",c:1,s:0,1\n");

    RUN(print << "plugins.itemtests.keys('HOME', 'CTRL+DOWN')" << print, tab1 + ",c:1,s:0,1\n" + tab1 + ",c:0,s:1,0\n");
    RUN(print, tab1 + ",c:1,s:1\n");

    // Selection stays consistent when removing items
    RUN(args << "setCurrentTab" << tab1 << "selectItems" << "1" << "2" << "3", "true\n");
    RUN(print << "remove(2)" << print, tab1 + ",c:3,s:1,2,3\n" + tab1 + ",c:2,s:1,-1,2\n");
    RUN(print, tab1 + ",c:2,s:1,2\n");

    // Renaming tab invalidates selection and all items because the tab
    // underlying data needs to be loaded again using plugins.
    const QString tab2 = testTab(2);
    const auto rename = QString("renameTab('%1', '%2')").arg(tab1, tab2);
    RUN(print << rename << print, tab1 + ",c:2,s:1,2\n" + tab1 + ",c:-1,s:\n");

    RUN(print, tab2 + ",c:0,s:0\n");
}

void Tests::synchronizeInternalCommands()
{
    // Keep internal commands synced with the latest version
    // but allow user to change some attributes.
    const auto script = R"(
        setCommands([
            {
                internalId: 'copyq_global_toggle',
                enable: false,
                icon: 'icon.png',
                shortcuts: ['Ctrl+F1'],
                globalShortcuts: ['Ctrl+F2'],
                name: 'Old name',
                cmd: 'Old command',
            },
        ])
        )";
    RUN(script, "");
    RUN("commands()[0].internalId", "copyq_global_toggle\n");
    RUN("commands()[0].enable", "false\n");
    RUN("commands()[0].icon", "icon.png\n");
    RUN("commands()[0].shortcuts", "Ctrl+F1\n");
    RUN("commands()[0].globalShortcuts", "Ctrl+F2\n");
    RUN("commands()[0].name", "Show/hide main window\n");
    RUN("commands()[0].cmd", "copyq: toggle()\n");
}

void Tests::queryKeyboardModifiersCommand()
{
    RUN("queryKeyboardModifiers()", "");
    // TODO: Is there a way to press modifiers?
}

void Tests::pointerPositionCommand()
{
    QCursor::setPos(1, 2);
    RUN("pointerPosition", "1\n2\n");
    QCursor::setPos(2, 3);
    RUN("pointerPosition", "2\n3\n");
}

void Tests::setPointerPositionCommand()
{
    RUN("setPointerPosition(1,2)", "");
    QCOMPARE(QPoint(1, 2), QCursor::pos());
    RUN("setPointerPosition(2,3)", "");
    QCOMPARE(QPoint(2, 3), QCursor::pos());
}

void Tests::setTabName()
{
    const auto script = R"(
        tab('1')
        add(1)
        tab1Size = size()

        tab('2')
        tab2Size = size()

        print(tab1Size + ',' + tab2Size)
        )";
    RUN(script, "1,0");
}

void Tests::abortInputReader()
{
    QProcess p;
    p.start(
        m_test->executable(),
        {"afterMilliseconds(250, abort); input(); print('DONE'); 'DONE'"}
    );
    QVERIFY2( p.waitForStarted(10000), "Process failed to start" );
    QVERIFY2( p.waitForFinished(5000), "Process failed to finish" );
    QCOMPARE( p.readAllStandardOutput(), "" );
}

void Tests::changeAlwaysOnTop()
{
    // The window should be still visible and focused after changing always-on-top flag.
    RUN("visible", "true\n");
    RUN("focused", "true\n");
    RUN("config" << "always_on_top", "false\n");

    RUN("config" << "always_on_top" << "true", "true\n");
    WAIT_ON_OUTPUT("visible", "true\n");
    // There is a problem activating the window again after
    // changing the always-on-top flag on macOS with Qt 6.
#if !defined(Q_OS_MAC) || QT_VERSION < QT_VERSION_CHECK(6,0,0)
    WAIT_ON_OUTPUT("focused", "true\n");
#endif

    RUN("config" << "always_on_top" << "false", "false\n");
    WAIT_ON_OUTPUT("visible", "true\n");
#if !defined(Q_OS_MAC) || QT_VERSION < QT_VERSION_CHECK(6,0,0)
    WAIT_ON_OUTPUT("focused", "true\n");
#endif

    RUN("hide", "");
    RUN("visible", "false\n");
    RUN("focused", "false\n");

    RUN("config" << "always_on_top" << "true", "true\n");
    WAIT_ON_OUTPUT("visible", "false\n");
    WAIT_ON_OUTPUT("focused", "false\n");

    RUN("config" << "always_on_top" << "false", "false\n");
    WAIT_ON_OUTPUT("visible", "false\n");
    WAIT_ON_OUTPUT("focused", "false\n");
}

void Tests::networkGet()
{
    SKIP_ON_ENV("COPYQ_TESTS_NO_NETWORK");

    RUN("r = networkGet('https://www.example.com'); r.status", "200\n");
}

void Tests::networkPost()
{
    SKIP_ON_ENV("COPYQ_TESTS_NO_NETWORK");

    const auto script = R"(
        r = NetworkRequest();
        r.headers['Content-Type'] = 'text/plain';
        s = r.request('POST', 'https://httpcan.org/post?hello=1', 'Hello');
        json = s.data;
        try {
            data = JSON.parse(str(json));
            userAgent = data.headers['user-agent'].replace(/\\/.*/, '/xyz');
            [data.data, JSON.stringify(data.args), userAgent, s.status];
        } catch (e) {
            [`Error parsing JSON response: ${e}\n`, json, s.status];
        }
    )";
    RUN(script, "Hello\n{\"hello\":\"1\"}\nCopyQ/xyz\n200\n");
}

void Tests::networkHeaders()
{
    // Default user-agent is "CopyQ/<version>"
    RUN("print(NetworkRequest().headers['User-Agent'])", copyqUserAgent());
    RUN("r = NetworkRequest(); r.headers['X'] = 'Y'; r.headers['X']", "Y\n");
}

void Tests::networkRedirects()
{
    SKIP_ON_ENV("COPYQ_TESTS_NO_NETWORK");

    RUN("r = networkGet('https://google.com'); r.status", "301\n");
    const auto script = R"(
        r = NetworkRequest();
        r.maxRedirects = 1;
        s = r.request('GET', 'https://google.com');
        [s.status, s.url]
    )";
    RUN(script, "200\nhttps://www.google.com/\n");
}

void Tests::networkGetPostAsync()
{
    RUN("r = networkGetAsync('copyq-test://example.com'); print([r.finished,r.error,r.finished])",
        "false,Protocol \"copyq-test\" is unknown,true");
    RUN("r = networkPostAsync('copyq-test://example.com'); print([r.finished,r.error,r.finished])",
        "false,Protocol \"copyq-test\" is unknown,true");
}

void Tests::pluginNotInstalled()
{
    RUN_EXPECT_ERROR_WITH_STDERR(
        "plugins.bad_plugin", CommandException,
        "Plugin \"bad_plugin\" is not installed"
    );
}

void Tests::startServerAndRunCommand()
{
    RUN("--start-server" << "tab" << testTab(1) << "write('TEST');read(0)", "TEST");

    TEST( m_test->stopServer() );

    QByteArray stdoutActual;
    QByteArray stderrActual;

    QCOMPARE( run(Args("--start-server") << "tab" << testTab(1) << "read" << "0", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QCOMPARE(stdoutActual, "TEST");

    QCOMPARE( run(Args() << "tab" << testTab(1) << "read" << "0", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QCOMPARE(stdoutActual, "TEST");

    // The sleep() call ensures that the server finishes and terminates the
    // client connection.
    QCOMPARE( run(Args("--start-server") << "exit();sleep(10000)", &stdoutActual, &stderrActual), 0 );
    QCOMPARE(stdoutActual, "");
    QVERIFY2( stderrActual.contains("Terminating server.\n"), stderrActual );

    // Try to start new client.
    SleepTimer t(10000);
    while ( run(Args("exit();sleep(10000)")) == 0 && t.sleep() ) {}
}

void Tests::avoidStoringPasswords()
{
    TEST( m_test->setClipboard(secretData("secret")) );
    WAIT_ON_OUTPUT("clipboard", "secret");
    RUN("read" << "0" << "1" << "2", "\n\n");
    RUN("count", "0\n");

    KEYS(clipboardBrowserId << keyNameFor(QKeySequence::Paste));
    waitFor(waitMsPasteClipboard);
    RUN("read" << "0" << "1" << "2", "secret\n\n");
    RUN("count", "1\n");
}

void Tests::scriptsForPasswords()
{
    const auto script = R"(
        setCommands([{
            isScript: true,
            cmd: `global.onSecretClipboardChanged = function() {
                add("SECRET");
            }`
        }])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "1\n");
    TEST( m_test->setClipboard(secretData("secret")) );
    WAIT_ON_OUTPUT("read" << "0" << "1" << "2", "SECRET\n\n");
    RUN("count", "1\n");
}

void Tests::currentClipboardOwner()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: 'global.currentClipboardOwner = function() { return settings("clipboard_owner"); }'
            },
            {
                automatic: true,
                input: mimeWindowTitle,
                cmd: 'copyq: setData("application/x-copyq-owner-test", input())',
            },
            {
                automatic: true,
                wndre: '.*IGNORE',
                cmd: 'copyq ignore; copyq add IGNORED',
            },
        ])
        )";
    RUN("settings" << "clipboard_owner" << "TEST1", "");
    RUN(script, "");

    TEST( m_test->setClipboard("test1") );
    WAIT_ON_OUTPUT("read(0)", "test1");
    RUN("read('application/x-copyq-owner-test', 0)", "TEST1");

    RUN("settings" << "clipboard_owner" << "TEST2", "");
    RUN("config" << "update_clipboard_owner_delay_ms" << "10000", "10000\n");
    TEST( m_test->setClipboard("test2") );
    WAIT_ON_OUTPUT("read(0)", "test2");
    RUN("read('application/x-copyq-owner-test', 0)", "TEST2");

    RUN("settings" << "clipboard_owner" << "TEST3", "");
    TEST( m_test->setClipboard("test3") );
    WAIT_ON_OUTPUT("read(0)", "test3");
    RUN("read('application/x-copyq-owner-test', 0)", "TEST2");

    RUN("settings" << "clipboard_owner" << "TEST4_IGNORE", "");
    RUN("config" << "update_clipboard_owner_delay_ms" << "0", "0\n");
    TEST( m_test->setClipboard("test4") );
    WAIT_ON_OUTPUT("read(0)", "IGNORED");

    RUN("settings" << "clipboard_owner" << "TEST5", "");
    TEST( m_test->setClipboard("test5") );
    WAIT_ON_OUTPUT("read(0)", "test5");
    RUN("read('application/x-copyq-owner-test', 0)", "TEST5");
}

void Tests::saveLargeItem()
{
    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;

    const auto script = R"(
        write(0, [{
            'text/plain': '1234567890'.repeat(10000),
            'application/x-copyq-test-data': 'abcdefghijklmnopqrstuvwxyz'.repeat(10000),
        }])
        )";
    RUN(args << script, "");

    for (int i = 0; i < 2; ++i) {
        RUN(args << "read(0).left(20)", "12345678901234567890");
        RUN(args << "read(0).length", "100000\n");
        RUN(args << "getItem(0)[mimeText].length", "100000\n");
        RUN(args << "getItem(0)[mimeText].left(20)", "12345678901234567890");
        RUN(args << "getItem(0)['application/x-copyq-test-data'].left(26)", "abcdefghijklmnopqrstuvwxyz");
        RUN(args << "getItem(0)['application/x-copyq-test-data'].length", "260000\n");
        RUN(args << "ItemSelection().selectAll().itemAtIndex(0)[mimeText].length", "100000\n");
        RUN("unload" << tab, tab + "\n");
    }

    RUN("show" << tab, "");
    KEYS(clipboardBrowserId << keyNameFor(QKeySequence::Copy));
    WAIT_ON_OUTPUT("clipboard().left(20)", "12345678901234567890");
    RUN("clipboard('application/x-copyq-test-data').left(26)", "abcdefghijklmnopqrstuvwxyz");
    RUN("clipboard('application/x-copyq-test-data').length", "260000\n");

    const auto tab2 = testTab(2);
    const auto args2 = Args("tab") << tab2;
    RUN("show" << tab2, "");
    waitFor(waitMsPasteClipboard);
    KEYS(clipboardBrowserId << keyNameFor(QKeySequence::Paste));
    RUN(args2 << "read(0).left(20)", "12345678901234567890");
    RUN(args2 << "read(0).length", "100000\n");
    RUN(args << "getItem(0)['application/x-copyq-test-data'].left(26)", "abcdefghijklmnopqrstuvwxyz");
    RUN(args << "getItem(0)['application/x-copyq-test-data'].length", "260000\n");
}

void Tests::clipboardUriList()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: 'global.clipboardFormatsToSave = function() { return [mimeUriList] }'
            },
        ])
        )";

    RUN(script, "");
    WAIT_ON_OUTPUT("commands().length", "1\n");

    const QByteArray uri = "https://test1.example.com";
    TEST( m_test->setClipboard(uri, mimeUriList) );
    WAIT_ON_OUTPUT("clipboard(mimeUriList)", uri);
}

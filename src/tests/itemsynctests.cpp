// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemsynctests.h"

#include "common/commandstatus.h"
#include "common/mimetypes.h"
#include "common/sleeptimer.h"
#include "tests/test_utils.h"
#include "gui/fromiconid.h"
#include "gui/icons.h"

#include <QDir>
#include <QFile>
#include <QUrl>

#include <memory>

namespace {

using FilePtr = std::shared_ptr<QFile>;

const char sep[] = " ;; ";

const auto confirmRemoveDialogId =
    "focus::QPushButton'Yes'<qt_msgbox_buttonbox:QDialogButtonBox<:QMessageBox"
    "'Do you really want to remove items and associated files\\\\?'";

class TestDir final {
public:
    explicit TestDir(int i, bool createPath = true)
        : m_dir(ItemSyncTests::testDir(i))
    {
        clear();
        if (createPath)
            create();
    }

    ~TestDir()
    {
        clear();
    }

    void clear()
    {
        if (isValid())
            m_dir.removeRecursively();
    }

    void create()
    {
        if ( !m_dir.mkpath(".") )
            Q_ASSERT(false);
    }

    bool isValid() const
    {
        return m_dir.exists();
    }

    QStringList files() const
    {
        QStringList files = m_dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
        files.removeOne(QStringLiteral(".copyq_lock"));
        return files;
    }

    FilePtr file(const QString &fileName) const
    {
        return std::make_shared<QFile>(filePath(fileName));
    }

    QString filePath(const QString &fileName) const
    {
        return m_dir.absoluteFilePath(fileName);
    }

    bool remove(const QString &fileName)
    {
        return QFile::remove(filePath(fileName));
    }

private:
    Q_DISABLE_COPY(TestDir)
    QDir m_dir;
};

QByteArray createFile(const TestDir &dir, const QString &fileName, const QByteArray &content)
{
    FilePtr file(dir.file(fileName));
    if ( file->exists() )
        return "File already exists!";

    if ( !file->open(QIODevice::WriteOnly) )
        return "Cannot open file!";

    if ( file->write(content) == -1 )
        return "Cannot write to file!";

    file->close();
    return "";
}

} // namespace

ItemSyncTests::ItemSyncTests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
    setProperty("CopyQ_test_id", QStringLiteral("itemsync"));
    m_test->setEnv("COPYQ_SYNC_UPDATE_INTERVAL_MS", "100");

    QStringList tabPaths;
    for (int i = 0; i < 10; ++i) {
        tabPaths.append(ItemSyncTests::testTab(i));
        tabPaths.append(ItemSyncTests::testDir(i));
    }

    QVariantList formatSettings;
    QVariantMap format;

    format["formats"] = QStringList() << "xxx";
    format["itemMime"] = QString(COPYQ_MIME_PREFIX "test-xxx");
    format["icon"] = QString(fromIconId(IconTrash));
    formatSettings << format;

    format["formats"] = QStringList() << "zzz" << ".yyy";
    format["itemMime"] = QString(COPYQ_MIME_PREFIX "test-zzz");
    format["icon"] = QString();
    formatSettings << format;

    QVariantMap settings;
    settings["sync_tabs"] = tabPaths;
    settings["format_settings"] = formatSettings;

    setProperty("CopyQ_test_settings", settings);
}

QString ItemSyncTests::testTab(int i)
{
    return ::testTab(i);
}

QString ItemSyncTests::testDir(int i)
{
    return QDir::tempPath() + "/copyq_test_dirs/itemsync_" + QString::number(i);
}

void ItemSyncTests::initTestCase()
{
    TEST(m_test->initTestCase());
}

void ItemSyncTests::cleanupTestCase()
{
    TEST(m_test->cleanupTestCase());
}

void ItemSyncTests::init()
{
    TEST(m_test->init());

    QDir tmpDir(QDir::cleanPath(testDir(0) + "/.."));
    if ( tmpDir.exists() )
        QVERIFY(tmpDir.removeRecursively());
}

void ItemSyncTests::cleanup()
{
    TEST( m_test->cleanup() );
}

void ItemSyncTests::createRemoveTestDir()
{
    TestDir dir1(1);
    TestDir dir2(2);

    QVERIFY(dir1.isValid());
    QCOMPARE(dir1.files().join(sep), QString());

    QVERIFY(dir2.isValid());
    QCOMPARE(dir2.files().join(sep), QString());

    const QString testFileName1 = "test1.txt";
    FilePtr f1(dir1.file(testFileName1));
    QVERIFY(!f1->exists());
    QVERIFY(f1->open(QIODevice::WriteOnly));
    f1->close();

    QCOMPARE(dir1.files().join(sep), testFileName1);

    dir1.clear();
    QVERIFY(!dir1.isValid());
    QVERIFY(!f1->exists());
    QVERIFY(dir2.isValid());

    dir2.clear();
    QVERIFY(!dir1.isValid());
    QVERIFY(!dir2.isValid());

    dir1.create();
    QVERIFY(dir1.isValid());
    QCOMPARE(dir2.files().join(sep), QString());
}

void ItemSyncTests::itemsToFiles()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C", "");
    RUN(args << "read" << "0" << "1" << "2", "C\nB\nA");
    RUN(args << "size", "3\n");

    const auto files = dir1.files();
    QVERIFY2( files.size() == 3, files.join(sep).toUtf8() );
    QVERIFY2( files[0].startsWith("copyq_"), files[0].toUtf8() );
    QVERIFY2( files[1].startsWith("copyq_"), files[1].toUtf8() );
    QVERIFY2( files[2].startsWith("copyq_"), files[2].toUtf8() );
}

void ItemSyncTests::filesToItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args()
        << "tab" << tab1
        << "read.apply(this, [...Array(size()).keys()])";

    RUN(args, "");

    QStringList expectedItems;

    const QByteArray text1 = "TEST1";
    TEST(createFile(dir1, "test1.txt", text1));
    // Hidden files (starting with dot) should be ignored
    const QByteArray hiddenFile = "HIDDEN";
    TEST(createFile(dir1, ".hidden.txt", hiddenFile));
    const QByteArray text2 = "TEST2";
    TEST(createFile(dir1, "test2.txt", text2));
    // Non-owned files are sorted by name
    expectedItems = QStringList{text1, text2};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));

    const QByteArray nested1_1 = "nested 1 item 1";
    QVERIFY(QDir(dir1.filePath("nested1")).mkpath("."));
    TEST(createFile(dir1, "nested1/test1.txt", nested1_1));
    // More nested items come later
    expectedItems = QStringList{text1, text2, nested1_1};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));

    const QByteArray text3 = "TEST3";
    TEST(createFile(dir1, "test3.txt", text3));
    expectedItems = QStringList{text1, text2, text3, nested1_1};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));

    const QByteArray text0 = "TEST0";
    TEST(createFile(dir1, "test0.txt", text0));
    expectedItems = QStringList{text0, text1, text2, text3, nested1_1};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));

    const QByteArray nested2_1 = "nested 2 item 1";
    QVERIFY(QDir(dir1.filePath("nested2")).mkpath("."));
    TEST(createFile(dir1, "nested2/test1.txt", nested2_1));
    expectedItems = QStringList{text0, text1, text2, text3, nested1_1, nested2_1};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));

    const QByteArray nested1_0 = "nested 1 item 0";
    TEST(createFile(dir1, "nested1/test0.txt", nested1_0));
    expectedItems = QStringList{
        text0, text1, text2, text3, nested1_0, nested1_1, nested2_1};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));

    const QByteArray nested2_0 = "nested 2 item 0";
    TEST(createFile(dir1, "nested2/test0.txt", nested2_0));
    expectedItems = QStringList{
        text0, text1, text2, text3, nested1_0, nested1_1, nested2_0, nested2_1};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));

    const QByteArray nested1_1_0 = "nested 1 nested 1 item 0";
    QVERIFY(QDir(dir1.filePath("nested1/nested1")).mkpath("."));
    TEST(createFile(dir1, "nested1/nested1/test0.txt", nested1_1_0));
    expectedItems = QStringList{
        text0, text1, text2, text3, nested1_0, nested1_1, nested2_0, nested2_1, nested1_1_0};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));

    SKIP_ON_ENV("COPYQ_TESTS_NO_SYMLINKS");
    // Ignore symlink to already added directory
    QFile dirLink(dir1.filePath("nested1"));
    if (!dirLink.link(dir1.filePath("nested3")))
        QFAIL("Failed to create directory link: " + dirLink.errorString().toUtf8());
    // Allow other symlinks
    QFile fileLink(dir1.filePath("test1.txt"));
    if (!fileLink.link(dir1.filePath("test4.txt")))
        QFAIL("Failed to create file link: " + fileLink.errorString().toUtf8());
    expectedItems = QStringList{
        text0, text1, text2, text3, text1, nested1_0, nested1_1, nested2_0, nested2_1, nested1_1_0};
    WAIT_ON_OUTPUT(args, expectedItems.join('\n'));
}

void ItemSyncTests::removeOwnItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C" << "D", "");

    const auto files = dir1.files();
    QVERIFY2( files.size() == 4, files.join(sep).toUtf8() );
    QVERIFY2( files[0].startsWith("copyq_"), files[0].toUtf8() );
    QVERIFY2( files[1].startsWith("copyq_"), files[1].toUtf8() );
    QVERIFY2( files[2].startsWith("copyq_"), files[2].toUtf8() );
    QVERIFY2( files[3].startsWith("copyq_"), files[3].toUtf8() );

    // Move to test tab and select second and third item.
    RUN("setCurrentTab" << tab1, "");
    RUN(args << "selectItems" << "1" << "2", "true\n");
    TEST_SELECTED(tab1.toUtf8() + " 2 1 2\n");

    // Remove selected items.
    KEYS(m_test->shortcutToRemove());
    RUN(args << "read" << "0" << "1" << "2" << "3", "D,A,,");
    QCOMPARE( dir1.files().join(sep),
              files[0]
              + sep + files[3]
              );

    // Removing own items works from script.
    RUN(args << "remove" << "1", "");
    RUN(args << "read" << "0" << "1" << "2" << "3", "D,,,");
    QCOMPARE( dir1.files().join(sep), files[3] );
}

void ItemSyncTests::removeNotOwnedItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    const QString fileA = "test1.txt";
    const QString fileB = "test2.txt";
    const QString fileC = "test3.txt";
    const QString fileD = "test4.txt";

    TEST(createFile(dir1, fileA, "A"));
    WAIT_ON_OUTPUT(args << "size", "1\n");
    TEST(createFile(dir1, fileB, "B"));
    WAIT_ON_OUTPUT(args << "size", "2\n");
    TEST(createFile(dir1, fileC, "C"));
    WAIT_ON_OUTPUT(args << "size", "3\n");
    TEST(createFile(dir1, fileD, "D"));
    WAIT_ON_OUTPUT(args << "size", "4\n");

    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileB
              + sep + fileC
              + sep + fileD
              );

    // Move to test tab and select second and third item.
    RUN("setCurrentTab" << tab1, "");
    RUN(args << "selectItems" << "1" << "2", "true\n");
    TEST_SELECTED(tab1.toUtf8() + " 2 1 2\n");

    // Accept the "Remove Items?" dialog.
    KEYS(clipboardBrowserId << m_test->shortcutToRemove() << confirmRemoveDialogId);
    KEYS(confirmRemoveDialogId << "ENTER" << clipboardBrowserId);
    RUN(args << "read" << "0" << "1" << "2" << "3", "A,D,,");
    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileD
              );

    // Removing not owned items from script doesn't work.
    RUN_EXPECT_ERROR(args << "remove" << "1", CommandException);
    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileD
              );
}

void ItemSyncTests::removeNotOwnedItemsCancel()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    const QString fileA = "test1.txt";
    const QString fileB = "test2.txt";

    TEST(createFile(dir1, fileA, "A"));
    WAIT_ON_OUTPUT(args << "size", "1\n");
    TEST(createFile(dir1, fileB, "B"));
    WAIT_ON_OUTPUT(args << "size", "2\n");

    QCOMPARE(dir1.files().join(sep), fileA + sep + fileB );

    // Move to test tab and select second item.
    RUN("setCurrentTab" << tab1, "");
    RUN(args << "selectItems" << "1", "true\n");
    TEST_SELECTED(tab1.toUtf8() + " 1 1\n");

    // Don't accept the "Remove Items?" dialog.
    KEYS(clipboardBrowserId << m_test->shortcutToRemove() << confirmRemoveDialogId);
    TEST_SELECTED(tab1.toUtf8() + " 1 1\n");
    KEYS(confirmRemoveDialogId << "ESCAPE" << clipboardBrowserId);
    RUN(args << "read" << "0" << "1", "A,B");
    QCOMPARE( dir1.files().join(sep), fileA + sep + fileB );
    TEST_SELECTED(tab1.toUtf8() + " 1 1\n");
}

void ItemSyncTests::removeFiles()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C" << "D", "");

    const auto files = dir1.files();
    QVERIFY2( files.size() == 4, files.join(sep).toUtf8() );
    QVERIFY2( files[0].startsWith("copyq_"), files[0].toUtf8() );
    QVERIFY2( files[1].startsWith("copyq_"), files[1].toUtf8() );
    QVERIFY2( files[2].startsWith("copyq_"), files[2].toUtf8() );
    QVERIFY2( files[3].startsWith("copyq_"), files[3].toUtf8() );

    FilePtr file = dir1.file(files[2]);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("C").data());
    file->remove();

    WAIT_ON_OUTPUT(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "D,B,A");

    dir1.file(files[1])->remove();
    dir1.file(files[0])->remove();

    WAIT_ON_OUTPUT(args << "size", "1\n");
    RUN(args << "read" << "0", "D");
}

void ItemSyncTests::modifyItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C" << "D", "");

    const auto files = dir1.files();
    QVERIFY2( files.size() == 4, files.join(sep).toUtf8() );

    FilePtr file = dir1.file(files[2]);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("C").data());
    file->close();

    KEYS("HOME" << "DOWN" << "F2" << ":XXX" << "F2");
    RUN(args << "size", "4\n");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "D,XXX,B,A,");

    file = dir1.file(files[2]);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("XXX").data());
    file->close();

    const auto script = R"(
        setCommands([{
            name: 'Modify current item',
            inMenu: true,
            shortcuts: ['Ctrl+F1'],
            cmd: `
                copyq: item = selectedItemsData()[0]
                item[mimeText] = "ZZZ"
                setSelectedItemData(0, item)
            `
        }])
        )";
    RUN(script, "");
    KEYS("HOME" << "DOWN" << "CTRL+F1");
    WAIT_ON_OUTPUT(args << "read" << "1", "ZZZ");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "D,ZZZ,B,A,");
    RUN(args << "unload" << tab1, "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "D,ZZZ,B,A,");

    file = dir1.file(files[2]);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("ZZZ").data());
    file->close();
}

void ItemSyncTests::modifyFiles()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C" << "D", "");

    const auto files = dir1.files();
    QVERIFY2( files.size() == 4, files.join(sep).toUtf8() );
    QVERIFY2( files[0].startsWith("copyq_"), files[0].toUtf8() );
    QVERIFY2( files[1].startsWith("copyq_"), files[1].toUtf8() );
    QVERIFY2( files[2].startsWith("copyq_"), files[2].toUtf8() );
    QVERIFY2( files[3].startsWith("copyq_"), files[3].toUtf8() );

    FilePtr file = dir1.file(files[2]);
    QVERIFY(file->open(QIODevice::ReadWrite));
    QCOMPARE(file->readAll().data(), QByteArray("C").data());
    file->write("X");
    file->close();

    WAIT_ON_OUTPUT(args << "read" << "0" << "1" << "2" << "3", "D,CX,B,A");
    RUN(args << "size", "4\n");
}

void ItemSyncTests::itemToClipboard()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN("show" << tab1, "");

    const Args args = Args() << "tab" << tab1;

    RUN(args << "add" << "TESTING2" << "TESTING1", "");
    RUN(args << "read" << "0" << "1", "TESTING1\nTESTING2");

    RUN("config"
        << "activate_closes" << "false"
        << "activate_focuses" << "false"
        << "activate_pastes" << "false"
        ,
        "activate_closes=false\n"
        "activate_focuses=false\n"
        "activate_pastes=false\n"
        );

    // select second item and move to top
    RUN("config" << "move" << "true", "true\n");
    RUN(args << "selectItems" << "1", "true\n");
    KEYS("ENTER");
    RUN(args << "read" << "0" << "1", "TESTING2\nTESTING1");

    WAIT_FOR_CLIPBOARD("TESTING2");
    RUN("clipboard", "TESTING2");

    // select without moving
    RUN("config" << "move" << "0", "false\n");
    RUN(args << "selectItems" << "1", "true\n");
    KEYS("ENTER");
    RUN(args << "read" << "0" << "1", "TESTING2\nTESTING1");

    WAIT_FOR_CLIPBOARD("TESTING1");
    RUN("clipboard", "TESTING1");
}

void ItemSyncTests::notes()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << ";" << "tab" << tab1;

    RUN(args << "add" << "TEST1", "");

    KEYS("CTRL+N" << ":TEST2" << "F2"
      << "CTRL+N" << ":TEST3" << "F2");
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "TEST3;TEST2;TEST1");

    const auto files = dir1.files();
    QVERIFY2( files.size() == 3, files.join(sep).toUtf8() );
    QVERIFY2( files[0].startsWith("copyq_"), files[0].toUtf8() );
    QVERIFY2( files[1].startsWith("copyq_"), files[1].toUtf8() );
    QVERIFY2( files[2].startsWith("copyq_"), files[2].toUtf8() );

    KEYS("HOME" << "DOWN" << "SHIFT+F2" << ":NOTE1" << "F2");
    RUN(args << "read" << mimeItemNotes << "0" << "1" << "2", ";NOTE1;");

    // One new file for notes.
    const QStringList files2 = dir1.files();
    QCOMPARE( files2.size(), files.size() + 1 );
    QString fileNote;
    for (const auto &file : files2) {
        if ( !files.contains(file) ) {
            fileNote = file;
            break;
        }
    }

    // Read file with the notes.
    FilePtr file = dir1.file(fileNote);
    QVERIFY(file->open(QIODevice::ReadWrite));
    QCOMPARE(file->readAll().data(), QByteArray("NOTE1").data());

    // Modify notes.
    file->write("+NOTE2");
    file->close();

    WAIT_ON_OUTPUT(args << "read" << mimeItemNotes << "0" << "1" << "2", ";NOTE1+NOTE2;");
    RUN(args << "size", "3\n");

    // Remove notes.
    QVERIFY(file->remove());

    WAIT_ON_OUTPUT(args << "read" << mimeItemNotes << "0" << "1" << "2", ";;");
    RUN(args << "size", "3\n");
}

void ItemSyncTests::customFormats()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << ";" << "tab" << tab1;

    createFile(dir1, "test1.xxx", "TEST1");

    const auto mime1 = COPYQ_MIME_PREFIX "test-xxx";
    const auto mime2 = COPYQ_MIME_PREFIX "test-zzz";

    WAIT_ON_OUTPUT(args << "size", "1\n");
    RUN(args << "read" << mime1 << "0", "TEST1");

    createFile(dir1, "test2.yyy", "TEST2");

    WAIT_ON_OUTPUT(args << "size", "2\n");
    RUN(args << "read" << mime2 << "1", "TEST2");

    createFile(dir1, "test3.zzz", "TEST3");

    const auto script = QString(R"_(
        print('  size: ')
        print(size())
        print('  basename: ')
        print(read(plugins.itemsync.mimeBaseName, 0, 1, 2))
        print('  mime1: ')
        print(read('%1', 0, 1, 2))
        print('  mime2: ')
        print(read('%2', 0, 1, 2))
        )_")
        .arg(mime1)
        .arg(mime2);

    WAIT_ON_OUTPUT(
        args << script,
        "  size: 3"
        "  basename: test1;test2;test3"
        "  mime1: TEST1;;"
        "  mime2: ;TEST2;TEST3"
        );

    // Remove
    dir1.remove("test2.yyy");

    WAIT_ON_OUTPUT(
        args << script,
        "  size: 2"
        "  basename: test1;test3;"
        "  mime1: TEST1;;"
        "  mime2: ;TEST3;"
        );

    // Modify file
    FilePtr file = dir1.file("test1.xxx");
    QVERIFY(file->open(QIODevice::Append));
    file->write("UPDATE");
    file->close();

    WAIT_ON_OUTPUT(
        args << script,
        "  size: 2"
        "  basename: test1;test3;"
        "  mime1: TEST1UPDATE;;"
        "  mime2: ;TEST3;"
        );

    // Create item with custom data
    RUN(args << "write" << mime2 << "NEW_ITEM", "");

    RUN(args << "size", "3\n");

    const auto files = dir1.files();
    QVERIFY2( files[0].startsWith("copyq_"), files[0].toUtf8() );
    const QString fileData = QString(files[0]).replace("txt", "zzz");
    QVERIFY2( fileData.endsWith(".zzz"), fileData.toUtf8() );

    // Check data
    file = dir1.file(fileData);
    QVERIFY(file->exists());
    QVERIFY(file->open(QIODevice::ReadWrite));
    QCOMPARE(file->readAll().data(), "NEW_ITEM");

    // Modify data
    file->write("+UPDATE");
    file->close();

    WAIT_ON_OUTPUT(
        Args(args) << "read" << mime2 << "0" << "2" << mime1 << "1",
        "NEW_ITEM+UPDATE;TEST3;TEST1UPDATE");
    RUN(args << "size", "3\n");
}

void ItemSyncTests::getAbsoluteFilePath()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << ";" << "tab" << tab1;

    const auto code = QString(
            R"(
            var path = plugins.itemsync.tabPaths["%1"]
            var baseName = str(getItem(0)[plugins.itemsync.mimeBaseName])
            var absoluteFilePath = Dir(path).absoluteFilePath(baseName)
            print(absoluteFilePath)
            )")
            .arg(tab1);

    createFile(dir1, "test1.txt", QByteArray());
    WAIT_ON_OUTPUT(args << "size", "1\n");

    RUN(args << code, dir1.filePath("test1"));
}

void ItemSyncTests::addItemsWhenFull()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    const Args args = Args() << "separator" << ";" << "tab" << tab1;
    RUN(args << "show" << tab1, "");

    RUN("config" << "maxitems" << "2", "2\n");
    RUN(args << "add" << "A" << "B", "");
    RUN(args << "read" << "0" << "1" << "2", "B;A;");
    RUN(args << "add" << "C", "");
    RUN(args << "read" << "0" << "1" << "2", "C;B;");
}

void ItemSyncTests::addItemsWhenFullOmitDeletingNotOwned()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    const Args args = Args() << "separator" << ";" << "tab" << tab1;
    RUN(args << "show" << tab1, "");

    RUN("config" << "maxitems" << "1", "1\n");

    const QString testFileName1 = "test1.txt";
    createFile(dir1, testFileName1, "NOT-OWNED");
    WAIT_ON_OUTPUT(args << "size", "1\n");
    RUN(args << "read" << "0" << "1", "NOT-OWNED;");

    RUN(args << "add" << "A", "");
    RUN(args << "read" << "0" << "1", "A;");
    RUN(args << "remove" << "0", "");

    WAIT_ON_OUTPUT(args << "size", "1\n");
    RUN(args << "read" << "0" << "1", "NOT-OWNED;");

    FilePtr f1(dir1.file(testFileName1));
    QVERIFY(f1->exists());
}

void ItemSyncTests::moveOwnItemsSortsBaseNames()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    const QString testScript = R"(
        var baseNames = [];
        for (var i = 0; i < 4; ++i) {
            baseNames.push(str(read(plugins.itemsync.mimeBaseName, i)));
        }
        for (var i = 0; i < 3; ++i) {
            var j = i + 1;
            if (baseNames[i] <= baseNames[j]) {
                print("Failed test: baseNames["+i+"] > baseNames["+j+"]\\n");
                print("  Where baseNames["+i+"] = '" + baseNames[i] + "'\\n");
                print("        baseNames["+j+"] = '" + baseNames[j] + "'\\n");
            }
        }
    )";

    RUN(args << "add" << "A" << "B" << "C" << "D", "");
    RUN(args << "read(0,1,2,3)", "D,C,B,A");
    RUN(args << testScript, "");

    KEYS("END" << "CTRL+UP");
    RUN(args << "read(0,1,2,3)", "D,C,A,B");
    RUN(args << testScript, "");

    KEYS("DOWN" << "CTRL+UP");
    RUN(args << "read(0,1,2,3)", "D,C,B,A");
    RUN(args << testScript, "");

    KEYS("DOWN" << "SHIFT+UP" << "CTRL+UP");
    RUN(args << "read(0,1,2,3)", "D,B,A,C");
    RUN(args << testScript, "");

    KEYS("CTRL+UP");
    RUN(args << "read(0,1,2,3)", "B,A,D,C");
    RUN(args << testScript, "");

    KEYS("CTRL+DOWN");
    RUN(args << "read(0,1,2,3)", "D,B,A,C");
    RUN(args << testScript, "");

    KEYS("END" << "CTRL+HOME");
    RUN(args << "read(0,1,2,3)", "C,D,B,A");
    RUN(args << testScript, "");

    KEYS("END" << "UP" << "CTRL+HOME");
    RUN(args << "read(0,1,2,3)", "B,C,D,A");
    RUN(args << testScript, "");

    KEYS("HOME" << "CTRL+END");
    RUN(args << "read(0,1,2,3)", "C,D,A,B");
    RUN(args << testScript, "");
}

void ItemSyncTests::avoidDuplicateItemsAddedFromClipboard()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN("show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN("config" << "clipboard_tab" << tab1, tab1 + "\n");
    WAIT_ON_OUTPUT("isClipboardMonitorRunning", "true\n");

    TEST( m_test->setClipboard("one") );
    WAIT_ON_OUTPUT(args << "read(0,1,2,3)", "one,,,");

    TEST( m_test->setClipboard("two") );
    WAIT_ON_OUTPUT(args << "read(0,1,2,3)", "two,one,,");

    TEST( m_test->setClipboard("one") );
    WAIT_ON_OUTPUT(args << "read(0,1,2,3)", "one,two,,");
}

void ItemSyncTests::saveLargeItem()
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
        RUN(args << "getItem(0)[mimeText].left(20)", "12345678901234567890");
        RUN(args << "getItem(0)[mimeText].length", "100000\n");
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

void ItemSyncTests::sortItemsSimple()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "tab" << tab1 << "separator" << ",";

    RUN("config" << "maxitems" << "4", "4\n");
    const QString initScript = R"(
        add(1,2,0,3);
        read(0,1,2,3);
    )";
    RUN(args << initScript, "3,0,2,1");
    RUN(args << "size", "4\n");

    const QString sortScript = R"(
        let sel = ItemSelection().selectAll();
        let items = sel.itemsFormat(mimeText);
        serverLog("SORT BEGIN");
        sel.sort((i, j) => items[i] < items[j]);
        serverLog("SORT END");
        read(0,1,2,3);
    )";
    RUN(args << sortScript, "0,1,2,3");
    RUN(args << "size", "4\n");
}

void ItemSyncTests::sortItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "tab" << tab1 << "separator" << ",";

    RUN("config" << "maxitems" << "4", "4\n");
    const QString initScript = R"(
        add(
            {[mimeText]: 1, [mimeHtml]: "I"},
            {[mimeText]: 2, [mimeHtml]: "II"},
            {[mimeText]: 0, [mimeHtml]: "O"},
            {[mimeText]: 3, [mimeHtml]: "III"},
        );
        read(0,1,2,3);
    )";
    RUN(args << initScript, "3,0,2,1");
    RUN(args << "read(mimeHtml,0,1,2,3)", "III,O,II,I");
    RUN(args << "size", "4\n");

    const QString sortScript = R"(
        let sel = ItemSelection().selectAll();
        let items = sel.itemsFormat(mimeText);
        serverLog("SORT BEGIN");
        sel.sort((i, j) => items[i] < items[j]);
        serverLog("SORT END");
        read(0,1,2,3);
    )";
    RUN(args << sortScript, "0,1,2,3");
    RUN(args << "read(mimeHtml,0,1,2,3)", "O,I,II,III");
    RUN(args << "size", "4\n");
}

void ItemSyncTests::copyFiles()
{
    TestDir dir1(1);
    TestDir dir2(2);
    TestDir dir3(3);
    const QString tab1 = testTab(1);
    const QString tab2 = testTab(2);
    const QString tab3 = testTab(3);
    RUN("setCurrentTab" << tab1, "");

    const QString getFirstItemFormats = R"(
        Object.keys(getItem(0))
        .filter(x => !x.startsWith('application/x-copyq-'))
        .join('\\n')
    )";

    TEST(createFile(dir1, "test.txt", "TEST"));
    WAIT_ON_OUTPUT("tab" << tab1 << "size", "1\n");
    RUN("tab" << tab1 << "read(0)", "TEST");
    QCOMPARE(dir1.files().join(" "), "test.txt");

    // Copying synced item from another tab, creates a new item with the same
    // content and file name as original - not overwriting existing files.
    KEYS(clipboardBrowserId << keyNameFor(QKeySequence::Copy));
    WAIT_FOR_CLIPBOARD("TEST");
    RUN("setCurrentTab" << tab2, "");
    KEYS(clipboardBrowserId << keyNameFor(QKeySequence::Paste));
    RUN("tab" << tab2 << "size", "1\n");
    RUN("tab" << tab2 << "read(0)", "TEST");
#ifdef Q_OS_MAC
    RUN("tab" << tab2 << getFirstItemFormats, "public.utf16-plain-text\ntext/plain\n");
    QCOMPARE(dir2.files().join(" "), "test.txt test_copyq.dat");
#else
    RUN("tab" << tab2 << getFirstItemFormats, "text/plain\n");
    QCOMPARE(dir2.files().join(" "), "test.txt");
#endif

    KEYS(clipboardBrowserId << keyNameFor(QKeySequence::Paste));
    RUN("tab" << tab2 << "size", "2\n");
    RUN("tab" << tab2 << "read(0)", "TEST");
#ifdef Q_OS_MAC
    RUN("tab" << tab2 << getFirstItemFormats, "public.utf16-plain-text\ntext/plain\n");
    QCOMPARE(dir2.files().join(" "), "test-0001.txt test-0001_copyq.dat test.txt test_copyq.dat");
#else
    RUN("tab" << tab2 << getFirstItemFormats, "text/plain\n");
    QCOMPARE(dir2.files().join(" "), "test-0001.txt test.txt");
#endif

    // Adding a local file URI, creates a new item with the file content and
    // the same file name as original - not overwriting existing files.
    const QString fileUri = QUrl::fromLocalFile(dir1.filePath("test.txt")).toString();
    RUN("setCurrentTab" << tab3, "");
    RUN("tab" << tab3 << "add({[mimeUriList]: '" + fileUri + "'})", "");
    RUN("tab" << tab3 << "size", "1\n");
    RUN("tab" << tab3 << getFirstItemFormats, "text/plain\n");
    QCOMPARE(dir3.files().join(" "), "test.txt");

    RUN("tab" << tab3 << "add({[mimeUriList]: '" + fileUri + "'})", "");
    RUN("tab" << tab3 << "size", "2\n");
    RUN("tab" << tab3 << getFirstItemFormats, "text/plain\n");
    QCOMPARE(dir3.files().join(" "), "test-0001.txt test.txt");
}

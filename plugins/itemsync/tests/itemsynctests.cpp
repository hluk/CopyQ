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

#include "itemsynctests.h"

#include "common/mimetypes.h"
#include "tests/test_utils.h"

#include <QDir>
#include <QFile>

#include <memory>

namespace {

using FilePtr = std::shared_ptr<QFile>;

const char sep[] = " ;; ";

QString fileNameForId(int i)
{
    return QString("copyq_%1.txt").arg(i, 4, 10, QChar('0'));
}

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
        if (isValid()) {
            for ( const auto &fileName : files() )
                remove(fileName);
            m_dir.rmpath(".");
        }
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
        return m_dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
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
    m_test->setEnv("COPYQ_SYNC_UPDATE_INTERVAL_MS", "100");
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

    // Remove temporary directory.
    for (int i = 0; i < 10; ++i)
        TestDir(i, false);

    QDir tmpDir(QDir::cleanPath(testDir(0) + "/.."));
    if ( tmpDir.exists() )
        QVERIFY(tmpDir.rmdir("."));
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

    QCOMPARE( dir1.files().join(sep),
              fileNameForId(0) + sep + fileNameForId(1) + sep + fileNameForId(2) );
}

void ItemSyncTests::filesToItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "tab" << tab1;

    RUN(args << "size", "0\n");

    const QByteArray text1 = "Hello world!";
    createFile(dir1, "test1.txt", text1);

    QTest::qSleep(1200);

    const QByteArray text2 = "And hello again!";
    TEST(createFile(dir1, "test2.txt", text2));

    WAIT_ON_OUTPUT(args << "size", "2\n");
    // Newer files first.
    RUN(args << "read" << "0", text2);
    RUN(args << "read" << "1", text1);
}

void ItemSyncTests::removeOwnItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C" << "D", "");

    const QString fileA = fileNameForId(0);
    const QString fileB = fileNameForId(1);
    const QString fileC = fileNameForId(2);
    const QString fileD = fileNameForId(3);

    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileB
              + sep + fileC
              + sep + fileD
              );

    // Move to test tab and select second and third item.
    RUN("setCurrentTab" << tab1, "");
    RUN(args << "selectItems" << "1" << "2", "true\n");
    RUN(args << "testSelected", tab1.toUtf8() + " 2 1 2\n");

    // Remove selected items.
    RUN(args << "keys" << m_test->shortcutToRemove(), "");
    RUN(args << "read" << "0" << "1" << "2" << "3", "D,A,,");
    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileD
              );

    // Removing own items works from script.
    RUN(args << "remove" << "1", "");
    RUN(args << "read" << "0" << "1" << "2" << "3", "D,,,");
    QCOMPARE( dir1.files().join(sep), fileD );
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
    RUN(args << "testSelected", tab1.toUtf8() + " 2 1 2\n");

    // Don't accept the "Remove Items?" dialog.
    RUN(args << "keys" << m_test->shortcutToRemove(), "");
    RUN(args << "keys" << "ESCAPE", "");
    RUN(args << "read" << "0" << "1" << "2" << "3", "D,C,B,A");
    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileB
              + sep + fileC
              + sep + fileD
              );

    // Accept the "Remove Items?" dialog.
    RUN(args << "keys" << m_test->shortcutToRemove(), "");
    RUN(args << "keys" << "ENTER", "");
    RUN(args << "read" << "0" << "1" << "2" << "3", "D,A,,");
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

void ItemSyncTests::removeFiles()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C" << "D", "");

    const QString fileA = fileNameForId(0);
    const QString fileB = fileNameForId(1);
    const QString fileC = fileNameForId(2);
    const QString fileD = fileNameForId(3);

    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileB
              + sep + fileC
              + sep + fileD
              );

    FilePtr file = dir1.file(fileC);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("C").data());
    file->remove();

    WAIT_ON_OUTPUT(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "D,B,A");

    dir1.file(fileB)->remove();
    dir1.file(fileA)->remove();

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

    const QString fileC = fileNameForId(2);
    FilePtr file = dir1.file(fileC);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("C").data());
    file->close();

    RUN(args << "keys" << "HOME" << "DOWN" << "F2" << ":XXX" << "F2", "");
    RUN(args << "size", "4\n");
    RUN(args << "read" << "0" << "1" << "2" << "3", "D,XXX,B,A");

    file = dir1.file(fileC);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("XXX").data());
    file->close();
}

void ItemSyncTests::modifyFiles()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    RUN(Args() << "show" << tab1, "");

    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C" << "D", "");

    const QString fileA = fileNameForId(0);
    const QString fileB = fileNameForId(1);
    const QString fileC = fileNameForId(2);
    const QString fileD = fileNameForId(3);

    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileB
              + sep + fileC
              + sep + fileD
              );

    FilePtr file = dir1.file(fileC);
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
    RUN(args << "keys" << "ENTER", "");
    RUN(args << "read" << "0" << "1", "TESTING2\nTESTING1");

    WAIT_FOR_CLIPBOARD("TESTING2");
    RUN("clipboard", "TESTING2");

    // select without moving
    RUN("config" << "move" << "0", "false\n");
    RUN(args << "selectItems" << "1", "true\n");
    RUN(args << "keys" << "ENTER", "");
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

    RUN(args << "keys"
        << "CTRL+N" << ":TEST2" << "F2"
        << "CTRL+N" << ":TEST3" << "F2", "");
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "TEST3;TEST2;TEST1");

    const QString fileTest1 = fileNameForId(0);
    const QString fileTest2 = fileNameForId(1);
    const QString fileTest3 = fileNameForId(2);

    const QStringList files1 = QStringList() << fileTest1 << fileTest2 << fileTest3;

    QCOMPARE( dir1.files().join(sep), files1.join(sep) );

    RUN(args << "keys" << "HOME" << "DOWN" << "SHIFT+F2" << ":NOTE1" << "F2", "");
    RUN(args << "read" << mimeItemNotes << "0" << "1" << "2", ";NOTE1;");

    // One new file for notes.
    const QStringList files2 = dir1.files();
    QCOMPARE( files2.size(), files1.size() + 1 );
    QString fileNote;
    for (const auto &file : files2) {
        if ( !files1.contains(file) ) {
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
    RUN(args << "read" << mime2 << "0", "TEST2");

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
        "  basename: test3;test2;test1"
        "  mime1: ;;TEST1"
        "  mime2: TEST3;TEST2;"
        );

    // Remove
    dir1.remove("test2.yyy");

    WAIT_ON_OUTPUT(
        args << script,
        "  size: 2"
        "  basename: test3;test1;"
        "  mime1: ;TEST1;"
        "  mime2: TEST3;;"
        );

    // Modify file
    FilePtr file = dir1.file("test1.xxx");
    QVERIFY(file->open(QIODevice::Append));
    file->write("UPDATE");
    file->close();

    WAIT_ON_OUTPUT(
        args << script,
        "  size: 2"
        "  basename: test3;test1;"
        "  mime1: ;TEST1UPDATE;"
        "  mime2: TEST3;;"
        );

    // Create item with custom data
    RUN(args << "write" << mime2 << "NEW_ITEM", "");

    RUN(args << "size", "3\n");

    const QString fileData = QString(fileNameForId(0)).replace("txt", "zzz");

    // Check data
    file = dir1.file(fileData);
    QVERIFY(file->exists());
    QVERIFY(file->open(QIODevice::ReadWrite));
    QCOMPARE(file->readAll().data(), "NEW_ITEM");

    // Modify data
    file->write("+UPDATE");
    file->close();

    WAIT_ON_OUTPUT(
        Args(args) << "read" << mime2 << "0" << "1" << mime1 << "2",
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

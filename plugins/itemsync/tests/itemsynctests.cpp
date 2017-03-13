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

class TestDir {
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

void ItemSyncTests::removeItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
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
    RUN(args << "keys" << "RIGHT", "");
    RUN(args << "keys" << "HOME" << "DOWN" << "SHIFT+DOWN", "");
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

    // Removing items from script won't work.
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
    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(args << "add" << "A" << "B" << "C" << "D", "");

    const QString fileC = fileNameForId(2);
    FilePtr file = dir1.file(fileC);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("C").data());
    file->close();

    RUN(args << "keys" << "RIGHT" << "HOME" << "DOWN" << "F2" << ":XXX" << "F2", "");
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

void ItemSyncTests::notes()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);

    const Args args = Args() << "separator" << ";" << "tab" << tab1;

    RUN(args << "add" << "TEST1", "");

    RUN(args << "keys" << "LEFT"
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
    const QSet<QString> filesDiff = files2.toSet() - files1.toSet();
    QCOMPARE( filesDiff.size(), 1 );
    const QString fileNote = *filesDiff.begin();

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
    const Args args = Args() << "separator" << ";" << "tab" << tab1;

    const QByteArray data1 = "Custom format content";
    createFile(dir1, "test1.xxx", data1);

    WAIT_ON_OUTPUT(args << "size", "1\n");
    RUN(args << "keys" << "LEFT", "");
    RUN(args << "read" << COPYQ_MIME_PREFIX "test-xxx" << "0", data1);

    const QByteArray data2 = "Other custom format content";
    createFile(dir1, "test2.yyy", data2);

    WAIT_ON_OUTPUT(args << "size", "2\n");
    RUN(args << "read" << COPYQ_MIME_PREFIX "test-zzz" << "0", data2);

    const QByteArray data3 = "Last custom format content";
    createFile(dir1, "test3.zzz", data3);

    WAIT_ON_OUTPUT(args << "size", "3\n");
    RUN(args << "read" << COPYQ_MIME_PREFIX "test-zzz" << "0", data3);

    RUN(args << "read" << COPYQ_MIME_PREFIX "test-xxx" << "0" << "1" << "2",
        ";;" + data1);
    RUN(args << "read" << COPYQ_MIME_PREFIX "test-zzz" << "0" << "1" << "2",
        data3 + ";" + data2 + ";");
    RUN(args << "read" << COPYQ_MIME_PREFIX "test-zzz" << "0" << "1" << COPYQ_MIME_PREFIX "test-xxx" << "2",
        data3 + ";" + data2 + ";" + data1);

    // Remove
    dir1.remove("test2.yyy");

    WAIT_ON_OUTPUT(args << "size", "2\n");
    RUN(args << "read" << COPYQ_MIME_PREFIX "test-zzz" << "0" << COPYQ_MIME_PREFIX "test-xxx" << "1",
        data3 + ";" + data1);

    // Modify file
    const QByteArray data4 = " with update!";
    FilePtr file = dir1.file("test1.xxx");
    QVERIFY(file->open(QIODevice::Append));
    file->write(data4);
    file->close();

    WAIT_ON_OUTPUT(
        Args(args) << "read" << COPYQ_MIME_PREFIX "test-zzz" << "0" << COPYQ_MIME_PREFIX "test-xxx" << "1",
        data3 + ";" + data1 + data4);
    RUN(args << "size", "2\n");

    // Create item with custom data
    const QByteArray data5 = "New item data!";
    RUN(args << "write" << COPYQ_MIME_PREFIX "test-zzz" << data5, "");

    RUN(args << "size", "3\n");

    const QString fileData = QString(fileNameForId(0)).replace("txt", "zzz");

    // Check data
    const QByteArray data6 = " And another data!";
    file = dir1.file(fileData);
    QVERIFY(file->exists());
    QVERIFY(file->open(QIODevice::ReadWrite));
    QCOMPARE(file->readAll().data(), data5.data());

    // Modify data
    file->write(data6);
    file->close();

    WAIT_ON_OUTPUT(
        Args(args) << "read" << COPYQ_MIME_PREFIX "test-zzz" << "0" << "1" << COPYQ_MIME_PREFIX "test-xxx" << "2",
        data5 + data6 + ";" + data3 + ";" + data1 + data4);
    RUN(args << "size", "3\n");
}

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

#include <QDir>
#include <QTemporaryFile>
#include <QTest>

#define RUN(arguments, stdoutExpected) \
    TEST( m_test->runClient(arguments, stdoutExpected) );

namespace {

typedef QSharedPointer<QFile> FilePtr;
typedef QStringList Args;

const char sep[] = " ;; ";

/// Interval to wait (in ms) for items to synchronize from files.
const int waitMsFiles = 2500;

QString fileNameForId(int i)
{
    return QString("copyq_%1.txt").arg(i, 4, 10, QChar('0'));
}

class TestDir {
public:
    TestDir(int i)
        : m_dir(ItemSyncTests::testDir(i))
    {
        clear();
        create();
    }

    ~TestDir()
    {
        clear();
    }

    void clear()
    {
        if (isValid()) {
            foreach ( const QString &fileName, files() )
                remove(fileName);
            m_dir.rmpath(".");
        }
    }

    void create()
    {
        m_dir.mkpath(".");
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
        return FilePtr(new QFile(filePath(fileName)));
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
    return "ITEMSYNC_TEST_&" + QString::number(i);
}

QString ItemSyncTests::testDir(int i)
{
    return QDir::tempPath() + "/copyq_test/itemsync_" + QString::number(i);
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
    const QByteArray out = m_test->cleanup();
    QVERIFY2( out.isEmpty(), out );
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

    RUN(Args(args) << "add" << "0" << "1" << "2", "");

    QCOMPARE( dir1.files().join(sep),
              fileNameForId(0) + sep + fileNameForId(1) + sep + fileNameForId(2) );
}

void ItemSyncTests::filesToItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    const Args args = Args() << "tab" << tab1;

    RUN(Args(args) << "size", "0\n");

    const QByteArray text1 = "Hello world!";
    createFile(dir1, "test1.txt", text1);

    QTest::qSleep(1200);

    const QByteArray text2 = "And hello again!";
    TEST(createFile(dir1, "test2.txt", text2));

    QTest::qSleep(waitMsFiles);

    RUN(Args(args) << "size", "2\n");
    // Newer files first.
    RUN(Args(args) << "read" << "0", text2);
    RUN(Args(args) << "read" << "1", text1);
}

void ItemSyncTests::removeItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(Args(args) << "add" << "A" << "B" << "C" << "D", "");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "D,C,B,A");

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
    RUN(Args(args) << "keys" << "RIGHT", "");
    RUN(Args(args) << "keys" << "HOME" << "DOWN" << "SHIFT+DOWN", "");
    RUN(Args(args) << "selected", tab1.toLocal8Bit() + "\n2\n1\n2\n");

    // Don't accept the "Remove Items?" dialog.
    RUN(Args(args) << "keys" << m_test->shortcutToRemove(), "");
    RUN(Args(args) << "keys" << "ESCAPE", "");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "D,C,B,A");
    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileB
              + sep + fileC
              + sep + fileD
              );

    // Accept the "Remove Items?" dialog.
    RUN(Args(args) << "keys" << m_test->shortcutToRemove(), "");
    RUN(Args(args) << "keys" << "ENTER", "");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "D,A,,");
    QCOMPARE( dir1.files().join(sep),
              fileA
              + sep + fileD
              );

    SKIP("Removing files with \"remove\" command doesn't work!");
    RUN(Args(args) << "remove" << "1", "");
    QCOMPARE( dir1.files().join(sep), fileD );
}

void ItemSyncTests::removeFiles()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(Args(args) << "add" << "A" << "B" << "C" << "D", "");
    RUN(Args(args) << "size", "4\n");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "D,C,B,A");

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

    QTest::qSleep(waitMsFiles);

    RUN(Args(args) << "size", "3\n");
    RUN(Args(args) << "read" << "0" << "1" << "2", "D,B,A");

    dir1.file(fileB)->remove();
    dir1.file(fileA)->remove();

    QTest::qSleep(waitMsFiles);

    RUN(Args(args) << "size", "1\n");
    RUN(Args(args) << "read" << "0", "D");
}

void ItemSyncTests::modifyItems()
{
    TestDir dir1(1);
    const QString tab1 = testTab(1);
    const Args args = Args() << "separator" << "," << "tab" << tab1;

    RUN(Args(args) << "add" << "A" << "B" << "C" << "D", "");
    RUN(Args(args) << "size", "4\n");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "D,C,B,A");

    const QString fileC = fileNameForId(2);
    FilePtr file = dir1.file(fileC);
    QVERIFY(file->open(QIODevice::ReadOnly));
    QCOMPARE(file->readAll().data(), QByteArray("C").data());
    file->close();

    RUN(Args(args) << "keys" << "RIGHT" << "HOME" << "DOWN" << "F2" << ":XXX" << "F2", "");
    RUN(Args(args) << "size", "4\n");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "D,XXX,B,A");

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

    RUN(Args(args) << "add" << "A" << "B" << "C" << "D", "");
    RUN(Args(args) << "size", "4\n");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "D,C,B,A");

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

    QTest::qSleep(waitMsFiles);

    RUN(Args(args) << "size", "4\n");
    RUN(Args(args) << "read" << "0" << "1" << "2" << "3", "D,CX,B,A");
}

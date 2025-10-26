// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/appconfig.h"

void Tests::classByteArray()
{
    RUN("ByteArray", "");
    RUN("ByteArray('test')", "test");
    RUN("ByteArray(ByteArray('test'))", "test");
    RUN("typeof(ByteArray('test'))", "object\n");
    RUN("ByteArray('test') instanceof ByteArray", "true\n");
    RUN("b = ByteArray('0123'); b.chop(2); b", "01");
    RUN("ByteArray('0123').equals(ByteArray('0123'))", "true\n");
    RUN("ByteArray('0123').left(3)", "012");
    RUN("ByteArray('0123').mid(1, 2)", "12");
    RUN("ByteArray('0123').mid(1)", "123");
    RUN("ByteArray('0123').right(3)", "123");
    RUN("ByteArray('0123').remove(1, 2)", "03");
    RUN("ByteArray(' 01  23 ').simplified()", "01 23");
    RUN("ByteArray('0123').toBase64()", "MDEyMw==");
    RUN("ByteArray('ABCd').toLower()", "abcd");
    RUN("ByteArray('abcD').toUpper()", "ABCD");
    RUN("ByteArray(' 01  23 ').trimmed()", "01  23");
    RUN("b = ByteArray('0123'); b.truncate(2); b", "01");
    RUN("ByteArray('0123').toLatin1String() == '0123'", "true\n");
    RUN("ByteArray('0123').valueOf() == '0123'", "true\n");
    RUN("ByteArray(8).size()", "8\n");
    RUN("b = ByteArray(); b.length = 10; b.length", "10\n");

    // ByteArray implicitly converts to str.
    RUN("ByteArray('test') == 'test'", "true\n");
    RUN("ByteArray('test1') == 'test2'", "false\n");
    RUN("ByteArray('test') === 'test'", "false\n");
    RUN("ByteArray('a') + 'b'", "ab\n");
}

void Tests::classFile()
{
    RUN("var f = new File('/copyq_missing_file'); f.exists()", "false\n");
}

void Tests::classDir()
{
    RUN("var d = new Dir('/missing_directory/')"
        "; d.exists()"
        , "false\n"
        );

    const auto home = QDir::homePath();
    RUN("Dir().homePath()", home + "\n");
    RUN("Dir().home().path()", home + "\n");

    const auto root = QDir::rootPath();
    RUN("Dir().rootPath()", root + "\n");
    RUN("Dir().root().path()", root + "\n");

    const auto temp = QDir::tempPath();
    RUN("Dir().tempPath()", temp + "\n");
    RUN("Dir().temp().path()", temp + "\n");

    RUN("Dir().separator()", QString(QDir::separator()) + "\n");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QDir dir(tmpDir.path());
    const auto path = dir.path();
    const auto args = QString::fromLatin1("var d = new Dir('%1')").arg(path);

    RUN(args << "d.exists()", "true\n");
    RUN(args << "d.isReadable()", "true\n");
    RUN(args << "d.isAbsolute()", "true\n");
    RUN(args << "d.isRelative()", "false\n");
    RUN(args << "d.absolutePath()", path + "\n");
    RUN(args << "d.path()", path + "\n");
    RUN(args << "d.makeAbsolute()", "true\n");

    RUN(args << "d.mkdir('test')", "true\n");
    QVERIFY( QDir(dir.filePath("test")).exists() );
    RUN(args << "d.exists('test')", "true\n");

    RUN(args << "d.mkpath('a/b/c')", "true\n");
    QVERIFY( QDir(dir.filePath("a/b/c")).exists() );
    RUN(args << "d.exists('a/b/c')", "true\n");
    RUN(args << "d.filePath('a/b/c')", dir.filePath("a/b/c") + "\n");
    RUN(args << "d.relativeFilePath('" + path + "/a/b/c')", "a/b/c\n");
    RUN("Dir('" + path + "/test/../a//b/c/..').canonicalPath()", QDir(path + "/a/b").canonicalPath() + "\n");
    RUN(args << "d.setPath('" + path + "/a/b/c')" << "d.path()", dir.filePath("a/b/c") + "\n");

    RUN(args << "d.cd('a')" << "d.cd('b')", "true\n");
    RUN(args << "d.cd('x')", "false\n");
    RUN(args << "d.cd('a')" << "d.cd('b')" << "d.path()", dir.filePath("a/b") + "\n");
    RUN(args << "d.cd('a')" << "d.cd('..')" << "d.path()", path + "\n");
    RUN(args << "d.cd('a')" << "d.cdUp()" << "d.path()", path + "\n");

    RUN(args << "d.count()", QString::fromLatin1("%1\n").arg(dir.count()));
    RUN(args << "d.dirName()", QString::fromLatin1("%1\n").arg(dir.dirName()));

    RUN(args << "d.match(['a*'], 'test')", "false\n");
    RUN(args << "d.match(['t*'], 'test')", "true\n");
    RUN(args << "d.entryList()", ".\n..\na\ntest\n");
    RUN(args << "d.entryList(['t*'])", "test\n");
    RUN(args << "d.entryList(['t?st', 'a*'])", "a\ntest\n");

    RUN(args << "d.setNameFilters(['t?st', 'a*'])" << "d.nameFilters()", "t?st\na*\n");

    QFile f(dir.filePath("test.txt"));
    QVERIFY( f.open(QIODevice::WriteOnly) );
    f.close();
    RUN(args << "d.exists('test.txt')", "true\n");
    RUN(args << "d.absoluteFilePath('test.txt')", dir.filePath("test.txt") + "\n");
    RUN(args << "d.rename('test.txt', 'test2.txt')", "true\n");
    RUN(args << "d.exists('test2.txt')", "true\n");
    RUN(args << "d.remove('test2.txt')", "true\n");
    RUN(args << "d.exists('test2.txt')", "false\n");

    RUN(args << "d.rmdir('test')", "true\n");
    RUN(args << "d.exists('test')", "false\n");

    RUN("Dir().cleanPath('/a//b/../c/')", QDir::cleanPath("/a//b/../c/") + "\n");
}

void Tests::classTemporaryFile()
{
    RUN("var f = new TemporaryFile(); f.open()", "true\n");

    QByteArray err;

    for ( const auto autoRemove : {true, false} ) {
        QByteArray fileName;
        const auto script =
                QString(R"(
                        var f = new TemporaryFile()
                        if (!f.open())
                            throw 'Failed to open temporary file'

                        f.setAutoRemove(%1)
                        print(f.fileName())
                        )").arg(autoRemove);
        run(Args() << script, &fileName, &err);
        QVERIFY2( testStderr(err), err );

        QFile f( QString::fromUtf8(fileName) );
        QVERIFY( f.exists() != autoRemove );

        if (!autoRemove)
            f.remove();
    }

    QByteArray fileName;
    const auto scriptWrite = R"(
        var f = new TemporaryFile()
        if (!f.open())
            throw 'Failed to open temporary file'

        if (!f.write('LINE'))
            throw 'Failed to write to temporary file'

        f.setAutoRemove(false)
        print(f.fileName())
        )";
    run(Args() << scriptWrite, &fileName);
    QVERIFY2( testStderr(err), err );
    QVERIFY( QFile::exists(QString::fromUtf8(fileName)) );

    QByteArray out;
    const auto scriptRead = R"(
        var f = new File(str(input()))
        if (!f.openReadOnly())
            throw 'Failed to open file'

        print(''
            + ' exists()=' + str(f.exists())
            + ' isOpen()=' + str(f.isOpen())
            + ' isReadable()=' + str(f.isReadable())
            + ' isWritable()=' + str(f.isWritable())
            + ' size()=' + str(f.size())
            + ' readAll()=' + str(f.readAll())
            + ' atEnd()=' + str(f.atEnd())
            + ' seek(0)=' + str(f.seek(0))
            + ' read(1)=' + str(f.read(1))
            + ' pos()=' + str(f.pos())
            + ' peek(1)=' + str(f.peek(1))
            + ' readLine()=' + str(f.readLine())
        )
        )";
    const QByteArray expectedOut =
        " exists()=true"
        " isOpen()=true"
        " isReadable()=true"
        " isWritable()=false"
        " size()=4"
        " readAll()=LINE"
        " atEnd()=true"
        " seek(0)=true"
        " read(1)=L"
        " pos()=1"
        " peek(1)=I"
        " readLine()=INE";
    run(Args() << scriptRead, &out, &err, fileName);
    QVERIFY2( testStderr(err), err );
    QCOMPARE(out, expectedOut);

    const auto scriptRemove = R"(
        var f = new File(str(input()))
        if (!f.remove())
            throw 'Failed to remove file'
        )";
    run(Args() << scriptRemove, &out, &err, fileName);
    QVERIFY2( testStderr(err), err );
    QCOMPARE(QByteArray(), out);
    QVERIFY( !QFile::exists(QString::fromUtf8(fileName)) );

    RUN("TemporaryFile().autoRemove()", "true\n");
    RUN("TemporaryFile().fileTemplate()", QDir::temp().filePath(QStringLiteral("copyq-%1.XXXXXX").arg(sessionName)) + "\n");
}

void Tests::classItemSelection()
{
    const auto tab1 = testTab(1);
    const Args args = Args("tab") << tab1 << "separator" << ",";
    const QString outRows("ItemSelection(tab=\"" + tab1 + "\", rows=[%1])\n");

    RUN(args << "add" << "C" << "B" << "A", "");
    RUN(args << "ItemSelection().length", "0\n");
    RUN("ItemSelection('" + tab1 + "').length", "0\n");
    RUN(args << "ItemSelection().selectAll().length", "3\n");
    RUN("ItemSelection('" + tab1 + "').selectAll().length", "3\n");

    RUN(args << "a = ItemSelection(); b = a; a === b", "true\n");
    RUN(args << "a = ItemSelection(); b = a.selectAll(); a === b", "true\n");

    RUN(args << "ItemSelection().selectAll().str()", outRows.arg("0..2"));
    RUN(args << "ItemSelection().selectRemovable().str()", outRows.arg("0..2"));
    RUN(args << "ItemSelection().selectRemovable().removeAll().str()", outRows.arg(""));
    RUN(args << "read(0,1,2)", ",,");

    RUN(args << "add" << "C" << "B" << "A", "");
    RUN(args << "ItemSelection().select(/A|C/).str()", outRows.arg("0,2"));
    RUN(args << "ItemSelection().select(/a|c/i).str()", outRows.arg("0,2"));
    RUN(args << "ItemSelection().select(/A/).select(/C/).str()", outRows.arg("0,2"));
    RUN(args << "ItemSelection().select(/C/).select(/A/).str()", outRows.arg("2,0"));
    RUN(args << "ItemSelection().select(/A|C/).invert().str()", outRows.arg("1"));

    RUN(args << "ItemSelection().select(/A|C/).deselectIndexes([0]).str()", outRows.arg("2"));
    RUN(args << "ItemSelection().select(/A|C/).deselectIndexes([1]).str()", outRows.arg("0"));
    RUN(args << "ItemSelection().select(/A|C/).deselectIndexes([0,1]).str()", outRows.arg(""));
    RUN(args << "ItemSelection().select(/A|C/).deselectSelection(ItemSelection().select(/A/)).str()", outRows.arg("2"));
    RUN(args << "ItemSelection().select(/A|C/).deselectSelection(ItemSelection().select(/C/)).str()", outRows.arg("0"));
    RUN(args << "ItemSelection().select(/A|C/).deselectSelection(ItemSelection().selectAll()).str()", outRows.arg(""));

    RUN(args << "a = ItemSelection().select(/a/i); b = a.copy(); a !== b", "true\n");
    RUN(args << "a = ItemSelection().select(/a/i); b = a.copy(); a.str() == b.str()", "true\n");
    RUN(args << "a = ItemSelection().select(/a|b/i); b = a.copy(); b.select(/C/); [a.rows(), '', b.rows()]", "0\n1\n\n0\n1\n2\n");

    RUN(args << "s = ItemSelection().selectAll(); insert(1, 'X'); insert(3, 'Y'); s.invert().str()", outRows.arg("1,3"));

    RUN(args << "ItemSelection().select(/a/i).invert().removeAll().str()", outRows.arg(""));
    RUN(args << "read(0,1,2)", "A,,");

    RUN(args << "ItemSelection().selectAll().removeAll().str()", outRows.arg(""));
    RUN(args << "read(0,1,2)", ",,");

    RUN(args << "write('application/x-tst', 'ghi')", "");
    RUN(args << "write('application/x-tst', 'def')", "");
    RUN(args << "write('application/x-tst', 'abc')", "");
    RUN(args << "read('application/x-tst',0,1,2)", "abc,def,ghi");
    RUN(args << "ItemSelection().select(/e/, 'application/x-tst').str()", outRows.arg("1"));
    RUN(args << "ItemSelection().select(/e/, 'application/x-tst').removeAll().str()", outRows.arg(""));
    RUN(args << "read('application/x-tst',0,1,2)", "abc,ghi,");

    RUN(args << "ItemSelection().selectAll().itemAtIndex(0)['application/x-tst']", "abc");
    RUN(args << "ItemSelection().selectAll().itemAtIndex(1)['application/x-tst']", "ghi");
    RUN(args << "ItemSelection().select(/h/, 'application/x-tst').itemAtIndex(0)['application/x-tst']", "ghi");
    RUN(args << "ItemSelection().select(/h/, 'application/x-tst').itemAtIndex(1)['application/x-tst'] == undefined", "true\n");

    RUN(args << "ItemSelection().select(/ghi/, 'application/x-tst').setItemAtIndex(0, {'application/x-tst': 'def'}).str()",
        outRows.arg("1"));
    RUN(args << "read('application/x-tst',0,1,2)", "abc,def,");

    RUN(args << "d = ItemSelection().selectAll().items(); [d.length, str(d[0]['application/x-tst']), str(d[1]['application/x-tst'])]", "2\nabc\ndef\n");
    RUN(args << "ItemSelection().selectAll().setItems([{'application/x-tst': 'xyz'}]).str()", outRows.arg("0,1"));
    RUN(args << "read('application/x-tst',0,1,2)", "xyz,def,");

    RUN(args << "ItemSelection().selectAll().setItemsFormat(mimeItemNotes, 'test1').str()", outRows.arg("0,1"));
    RUN(args << "read(mimeItemNotes,0,1,2)", "test1,test1,");
    RUN(args << "read('application/x-tst',0,1,2)", "xyz,def,");

    RUN(args << "ItemSelection().selectAll().setItems([{'application/x-tst': ByteArray('abc')}]).str()", outRows.arg("0,1"));
    RUN(args << "read('application/x-tst',0,1,2)", "abc,def,");

    RUN(args << "ItemSelection().selectAll().setItemsFormat(mimeItemNotes, ByteArray('test2')).str()", outRows.arg("0,1"));
    RUN(args << "read(mimeItemNotes,0,1,2)", "test2,test2,");
    RUN(args << "read('application/x-tst',0,1,2)", "abc,def,");

    RUN(args << "ItemSelection().selectAll().itemsFormat(mimeItemNotes).map(str)", "test2\ntest2\n");
    RUN(args << "ItemSelection().selectAll().itemsFormat('application/x-tst').map(str)", "abc\ndef\n");
    RUN(args << "ItemSelection().selectAll().itemsFormat(ByteArray('application/x-tst')).map(str)", "abc\ndef\n");

    RUN(args << "ItemSelection().selectAll().setItemsFormat(mimeItemNotes, undefined).str()", outRows.arg("0,1"));
    RUN(args << "read(mimeItemNotes,0,1,2)", ",,");
    RUN(args << "read('application/x-tst',0,1,2)", "abc,def,");

    RUN(args << "ItemSelection().selectAll().removeAll().str()", outRows.arg(""));
    RUN(args << "add" << "C" << "B" << "A", "");
    RUN(args << "ItemSelection().select(/C/).move(1).str()", outRows.arg("1"));
    RUN(args << "read(0,1,2)", "A,C,B");
    RUN(args << "ItemSelection().select(/B/).select(/C/).move(1).str()", outRows.arg("2,1"));
    RUN(args << "read(0,1,2)", "A,C,B");
    RUN(args << "ItemSelection().select(/A/).move(2).str()", outRows.arg("1"));
    RUN(args << "read(0,1,2)", "C,A,B");
    RUN(args << "ItemSelection().select(/C/).select(/B/).move(2).str()", outRows.arg("1,2"));
    RUN(args << "read(0,1,2)", "A,C,B");

    RUN(args << "change(1, mimeItemNotes, 'NOTE'); read(mimeItemNotes,0,1,2)", ",NOTE,");
    RUN(args << "ItemSelection().select(/.*/, mimeItemNotes).str()", outRows.arg("1"));
    RUN(args << "ItemSelection().select(undefined, mimeItemNotes).str()", outRows.arg("0,2"));

    // Match nothing if select() argument is not a regular expression.
    RUN(args << "add" << "", "");
    RUN(args << "ItemSelection().select('A').str()", outRows.arg(""));
}

void Tests::classItemSelectionGetCurrent()
{
    const auto tab1 = testTab(1);
    const Args args = Args("tab") << tab1 << "separator" << ",";

    RUN("ItemSelection().tab", "CLIPBOARD\n");
    RUN(args << "ItemSelection().tab", tab1 + "\n");

    RUN(args << "ItemSelection().current().tab", tab1 + "\n");
    RUN(args << "ItemSelection().current().str()", "ItemSelection(tab=\"" + tab1 + "\", rows=[])\n");
    RUN("setCurrentTab" << tab1, "");
    RUN(args << "ItemSelection().current().tab", tab1 + "\n");
    RUN(args << "ItemSelection().current().str()", "ItemSelection(tab=\"" + tab1 + "\", rows=[])\n");

    RUN(args << "add" << "C" << "B" << "A", "");
    RUN(args << "ItemSelection().current().str()", "ItemSelection(tab=\"" + tab1 + "\", rows=[0])\n");

    RUN("setCommands([{name: 'test', inMenu: true, shortcuts: ['Ctrl+F1'], cmd: 'copyq: add(ItemSelection().current().str())'}])", "");
    KEYS("CTRL+F1");
    WAIT_ON_OUTPUT(args << "read(0)", "ItemSelection(tab=\"" + tab1 + "\", rows=[0])");
    KEYS("END" << "SHIFT+UP" << "CTRL+F1");
    WAIT_ON_OUTPUT(args << "read(0)", "ItemSelection(tab=\"" + tab1 + "\", rows=[3,2])");
}

void Tests::classItemSelectionByteArray()
{
    const auto tab1 = testTab(1);
    const Args args = Args("tab") << tab1 << "separator" << ",";
    RUN("setCurrentTab" << tab1, "");

    RUN(args << "add" << "C" << "B" << "A", "");
    RUN(args << "ByteArray(ItemSelection().selectAll().itemAtIndex(0)[mimeText])", "A");
    RUN(args << "str(ItemSelection().selectAll().itemAtIndex(0)[mimeText])", "A\n");
    RUN(args << "write(0, [ItemSelection().selectAll().itemAtIndex(2)])"
             << "read(mimeText, 0)", "C");
}

void Tests::classItemSelectionSort()
{
    const auto tab1 = testTab(1);
    const Args args = Args("tab") << tab1 << "separator" << ",";
    const QString outRows("ItemSelection(tab=\"" + tab1 + "\", rows=[%1])\n");
    RUN("setCurrentTab" << tab1, "");

    const QString initScript = R"(
        add(
            {[mimeText]: 2, [mimeHtml]: "two"},
            {[mimeText]: 5, [mimeHtml]: "five"},
            {[mimeText]: 1, [mimeHtml]: "one"},
            {[mimeText]: 3, [mimeHtml]: "three"},
            {[mimeText]: 4, [mimeHtml]: "four"},
        );
        read(0,1,2,3,4);
    )";
    RUN(args << initScript, "4,3,1,5,2");

    const auto script = R"(
        var sel = ItemSelection().selectAll();
        const texts = sel.itemsFormat(mimeText);
        sel.sort(function(i,j){
            return texts[i] < texts[j];
        });
        sel.str();
    )";
    RUN(args << script, outRows.arg("3,2,0,4,1"));
    RUN(args << "read(0,1,2,3,4)", "1,2,3,4,5");
    RUN(args << "read(mimeHtml,0,1,2,3,4)", "one,two,three,four,five");
    RUN(args << "size", "5\n");
}

void Tests::classSettings()
{
    TemporaryFile configFile;
    const QString fileName = configFile.fileName();

    RUN("eval" << "s=Settings(str(arguments[1])); print(s.fileName())" << fileName, fileName);
    RUN("eval" << "s=Settings(str(arguments[1])); s.isWritable() === true" << fileName, "true\n");

    RUN("eval" << "s=Settings(str(arguments[1])); s.contains('o1')" << fileName, "false\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.setValue('o1', 1); s.sync(); s.contains('o1')" << fileName, "true\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.value('o1')" << fileName, "1\n");

    RUN("eval" << "s=Settings(str(arguments[1])); s.setValue('o2', 2)" << fileName, "");
    RUN("eval" << "s=Settings(str(arguments[1])); s.value('o2')" << fileName, "2\n");

    RUN("eval" << "s=Settings(str(arguments[1])); s.setValue('o2', [1,2,3])" << fileName, "");
    RUN("eval" << "s=Settings(str(arguments[1])); s.value('o2')[0]" << fileName, "1\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.value('o2')[1]" << fileName, "2\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.value('o2')[2]" << fileName, "3\n");

    RUN("eval" << "s=Settings(str(arguments[1])); s.setValue('g1/o3', true)" << fileName, "");
    RUN("eval" << "s=Settings(str(arguments[1])); s.value('g1/o3')" << fileName, "true\n");

    RUN("eval" << "s=Settings(str(arguments[1])); s.childKeys()" << fileName, "o1\no2\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.allKeys()" << fileName, "g1/o3\no1\no2\n");

    RUN("eval" << "s=Settings(str(arguments[1])); s.beginGroup('g1'); s.group()" << fileName, "g1\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.beginGroup('g1'); s.setValue('g1.2/o4', 'test')" << fileName, "");
    RUN("eval" << "s=Settings(str(arguments[1])); s.beginGroup('g1'); s.childGroups()" << fileName, "g1.2\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.beginGroup('g1'); s.endGroup(); s.childGroups()" << fileName, "g1\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.value('g1/g1.2/o4')" << fileName, "test\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.allKeys()" << fileName, "g1/g1.2/o4\ng1/o3\no1\no2\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.remove('g1/g1.2/o4'); s.allKeys()" << fileName, "g1/o3\no1\no2\n");

    RUN("eval" << "s=Settings(str(arguments[1])); s.beginWriteArray('a1', 3); s.setArrayIndex(1); s.setValue('o1', 'v1'); s.endArray()" << fileName, "");
    RUN("eval" << "s=Settings(str(arguments[1])); s.beginReadArray('a1')" << fileName, "3\n");
    RUN("eval" << "s=Settings(str(arguments[1])); s.beginReadArray('a1'); s.setArrayIndex(1); s.value('o1');" << fileName, "v1\n");

    RUN("eval" << "s=Settings(str(arguments[1])); s.clear(); s.allKeys()" << fileName, "");

    QVERIFY(QFile::remove(fileName));
    QVERIFY(!QFile::exists(fileName));
    RUN("eval" << "s=Settings(str(arguments[1])); s.setValue('o1', 1); s.sync(); File(str(arguments[1])).exists()" << fileName, "true\n");
    QVERIFY(QFile::exists(fileName));

    QVERIFY(QFile::remove(fileName));
    QVERIFY(!QFile::exists(fileName));
    RUN("eval" << "s=Settings(str(arguments[1])); s.setValue('o1', 1)" << fileName, "");
    QVERIFY(QFile::exists(fileName));

    const QString appConfigFileName = AppConfig().settings().fileName();
    RUN("Settings().fileName()", QStringLiteral("%1\n").arg(appConfigFileName));
    RUN("Settings().value('Options/tabs')", QStringLiteral("%1\n").arg(clipboardTabName));
}

void Tests::calledWithInstance()
{
    // These would fail with the old deprecated Qt Script module.
    RUN("f=ByteArray().size; f()", "0\n");
    RUN("f=Dir().path; f()", ".\n");
    RUN("f=File('test').fileName; f()", "test\n");
    RUN("f=TemporaryFile().autoRemove; f()", "true\n");
}

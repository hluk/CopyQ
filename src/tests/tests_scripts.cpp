// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/mimetypes.h"
#include "common/commandstatus.h"

#include <QRegularExpression>
#include <QTemporaryFile>

void Tests::commandExit()
{
    RUN("exit", "");

    TEST( m_test->waitForServerToStop() );

    QCOMPARE( run(Args("exit")), 1 );
}

void Tests::commandEval()
{
    RUN("eval" << "", "");
    RUN("eval" << "1", "1\n");
    RUN("eval" << "[1,2,3]", "1\n2\n3\n");
    RUN("eval" << "'123'", "123\n");
    RUN("eval" << "'123'", "123\n");

    RUN("-e" << "", "");
    RUN("-e" << "1", "1\n");
}

void Tests::commandEvalThrows()
{
    RUN_EXPECT_ERROR_WITH_STDERR(
        "throw Error('Some exception')", CommandException,
        "ScriptError: Some exception\n"
        "\n"
        "--- backtrace ---\n"
    );

    RUN_EXPECT_ERROR_WITH_STDERR(
        "throw 'Some exception'", CommandException,
        "ScriptError: Some exception\n"
    );

    RUN_EXPECT_ERROR("eval('throw Error(1)')", CommandException);
    RUN_EXPECT_ERROR("eval('throw 1')", CommandException);
    RUN_EXPECT_ERROR("eval" << "throw Error(1)", CommandException);
    RUN_EXPECT_ERROR("eval" << "throw 1", CommandException);
}

void Tests::commandEvalSyntaxError()
{
    RUN_EXPECT_ERROR_WITH_STDERR("eval" << "(", CommandException, "SyntaxError");
}

void Tests::commandEvalArguments()
{
    RUN("eval" << "str(arguments[1]) + ', ' + str(arguments[2])" << "Test 1" << "Test 2",
        "Test 1, Test 2\n");
}

void Tests::commandEvalEndingWithComment()
{
    /*
    With Qml scripts in Qt 5, it's not possible to get uncaught exceptions
    from `QJSEngine::evaluate()`.

    Workaround is to wrap the script properly in an try/catch block:

        try {
            %1
        } catch(e) {
            _store_exception_internally(e);
            throw e;
        }

    (Unfortunately, it's still possible to escape the function with a script injection.)
    */
    RUN("eval" << "1 // TEST", "1\n");
}

void Tests::commandPrint()
{
    RUN("print" << "1", "1");
    RUN("print" << "TEST", "TEST");
}

void Tests::commandAbort()
{
    RUN("abort(); 1", "");
    RUN("eval" << "abort(); 1", "");
    RUN("eval" << "eval('abort(); print(1)'); 2", "");
    RUN("eval" << "execute('copyq', 'eval', '--', 'abort(); print(1)'); 2", "2\n");
}

void Tests::commandFail()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("fail"), &stdoutActual, &stderrActual), 10 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QCOMPARE( stdoutActual, QByteArray() );
}

void Tests::commandSource()
{
    const auto script =
            R"(
            test = function() { return " TEST" }
            print("SOURCED")
            )";

    QTemporaryFile scriptFile;
    QVERIFY(scriptFile.open());
    scriptFile.write(script);
    scriptFile.close();
    const auto scriptFileName = scriptFile.fileName();

    RUN("source" << scriptFileName, "SOURCED");
    RUN("source" << scriptFileName << "test()", "SOURCED TEST\n");
}

void Tests::commandVisible()
{
    RUN("visible", "true\n");
}

void Tests::commandToggle()
{
    RUN("visible", "true\n");
    RUN("toggle", "false\n");
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("toggle", "true\n");
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::commandShowHide()
{
    RUN("visible", "true\n");
    RUN("hide", "");
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("show", "");
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::commandShowAt()
{
    RUN("visible", "true\n");
    RUN("hide", "");
    WAIT_ON_OUTPUT("visible", "false\n");

    RUN("showAt", "");
    WAIT_ON_OUTPUT("visible", "true\n");
}

void Tests::commandFocused()
{
    RUN("focused", "true\n");
    RUN("hide", "");
    RUN("focused", "false\n");
}

void Tests::commandsUnicode()
{
    const auto text = QString::fromUtf8(QByteArray("Zkouška s různými českými znaky!"));
    RUN_WITH_INPUT("eval" << "input()", text, text);
    RUN_WITH_INPUT("eval" << "str(input())", text, text + "\n");

    RUN_WITH_INPUT("eval" << "fromUnicode(str(input()), 'utf8')", text, text);
    RUN_WITH_INPUT("eval" << "toUnicode(fromUnicode(str(input()), 'utf16'), 'utf16')", text, text + "\n");
    RUN_WITH_INPUT("eval" << "toUnicode(fromUnicode(str(input()), 'utf32le'), 'utf32le')", text, text + "\n");
    RUN_WITH_INPUT("eval" << "toUnicode( fromUnicode(str(input()), 'utf16le') )", text, text + "\n");
}

void Tests::commandsAddRead()
{
    RUN("add" << "A", "");
    RUN("read" << "0", "A");

    RUN("add" << "B", "");
    RUN("read" << "0", "B");
    RUN("read" << "1", "A");

    RUN("add" << "C" << "D", "");
    RUN("read" << "0", "D");
    RUN("read" << "1", "C");
    RUN("read" << "2", "B");
    RUN("read" << "3", "A");
}

void Tests::commandsWriteRead()
{
    const QByteArray input("\x00\x01\x02\x03\x04", 5);
    const auto arg1 = QString::fromLatin1("\x01\x02\x03\x04");
    const auto arg2 = QString::fromLatin1("\x7f\x6f\x5f\x4f");
    TEST( m_test->runClient(
              Args() << "write"
              << COPYQ_MIME_PREFIX "test1" << arg1
              << COPYQ_MIME_PREFIX "test2" << "-"
              << COPYQ_MIME_PREFIX "test3" << arg2, "",
              input) );
    RUN("read" << COPYQ_MIME_PREFIX "test1" << "0", arg1.toLatin1());
    RUN("read" << COPYQ_MIME_PREFIX "test2" << "0", input);
    RUN("read" << COPYQ_MIME_PREFIX "test3" << "0", arg2.toLatin1());

    RUN("write(1, {'text/plain': 'A'}, {'text/plain': 'B'})", "");
    RUN("read(mimeText, 0, 1, 2, 3)", "\nB\nA\n");

    RUN("write(0, [{'text/plain': 'C'}, {'text/plain': 'D'}])", "");
    RUN("read(mimeText, 0, 1, 2, 3)", "D\nC\n\nB");

    RUN("write(0, ['E', 'F'])", "");
    RUN("read(mimeText, 0, 1, 2, 3)", "F\nE\nD\nC");

    RUN_EXPECT_ERROR_WITH_STDERR(
        "write(0, [{}], [{}])",
        CommandException, "Unexpected multiple item list arguments");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "write(0)",
        CommandException, "Expected item arguments");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "write(0, '1', '2', '3')",
        CommandException, "Unexpected uneven number of mimeType/data arguments");
}

void Tests::commandsReadUtf8ByDefault()
{
    RUN("add({[mimeText]: 'A', [mimeTextUtf8]: 'B'})", "");
    RUN("read(mimeText, 0)", "A");
    RUN("read(0)", "B");
}

void Tests::commandChange()
{
    RUN("add" << "C" << "B" << "A", "");
    RUN("change" << "1" << "text/plain" << "b", "");
    RUN("separator" << " " << "read" << "0" << "1" << "2", "A b C");

    RUN("change" << "1" << "text/plain" << "B" << "text/html" << "<b>B</b>", "");
    RUN("read" << "text/html" << "1", "<b>B</b>");
    RUN("separator" << " " << "read" << "0" << "1" << "2", "A B C");

    RUN("change(1, 'text/html', undefined)", "");
    RUN("read" << "?" << "1", "text/plain\n");
}

void Tests::commandSetCurrentTab()
{
    const auto tab = testTab(1);
    RUN("setCurrentTab" << tab, "");
    RUN("testSelected", tab + "\n");
}

void Tests::commandConfig()
{
    QByteArray stdoutActual;
    QByteArray stderrActual;
    QCOMPARE( run(Args("config"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );

    // invalid option
    RUN_EXPECT_ERROR_WITH_STDERR("config" << "xxx", CommandException, "xxx");

    QCOMPARE( run(Args("config") << "tab_tree", &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY2( stdoutActual == "true\n" || stdoutActual == "false\n", stdoutActual);

    RUN("config" << "tab_tree" << "true", "true\n");
    RUN("config" << "tab_tree", "true\n");

    RUN("config" << "tab_tree" << "false", "false\n");
    RUN("config" << "tab_tree", "false\n");

    RUN("config" << "tab_tree" << "1", "true\n");
    RUN("config" << "tab_tree", "true\n");

    RUN("config" << "tab_tree" << "0", "false\n");
    RUN("config" << "tab_tree", "false\n");

    // Set multiple options.
    RUN("config" << "tab_tree" << "0" << "text_wrap" << "1",
        "tab_tree=false\n"
        "text_wrap=true\n");

    // Don't set any options if there is an invalid one.
    RUN_EXPECT_ERROR_WITH_STDERR("config" << "tab_tree" << "1" << "xxx" << "0", CommandException, "xxx");
    RUN("config" << "tab_tree", "false\n");
}

void Tests::commandToggleConfig()
{
    RUN("toggleConfig" << "check_clipboard", "false\n");
    RUN("config" << "check_clipboard", "false\n");
    RUN("toggleConfig" << "check_clipboard", "true\n");
    RUN("config" << "check_clipboard", "true\n");

    RUN_EXPECT_ERROR("toggleConfig", CommandException);
    RUN_EXPECT_ERROR_WITH_STDERR("toggleConfig" << "xxx", CommandException, "xxx");
    RUN_EXPECT_ERROR_WITH_STDERR("toggleConfig" << "clipboard_tab", CommandException, "clipboard_tab");
}

void Tests::commandDialog()
{
    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('text')", "TEST\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QLineEdit in :QDialog" << ":TEST" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('text') === undefined", "true\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QLineEdit in :QDialog" << "ESCAPE", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('.defaultChoice', 2, 'list', [1, 2, 3])", "2\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QComboBox in :QDialog" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('.defaultChoice', '', 'list', [1, 2, 3])", "\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QComboBox in :QDialog" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('list', [0, 1, 2])", "0\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QComboBox in :QDialog" << "ENTER", ""); }
    );

    // Can't focus configuration checkboxes on OS X
#ifndef Q_OS_MAC
    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('boolean', true) === true", "true\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QCheckBox in :QDialog" << "ENTER", ""); }
    );
#endif

    // Verify that special argument ".title" changes dialog's object name
    // so that geometry can be stored.
    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('.title', 'test', 'text')", ""); },
        [&]() { RUN(Args() << "keys" << "focus::QLineEdit in dialog_test:QDialog" << "ESCAPE", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    const QByteArray script = R"(
        dialog(
            '.width', 100,
            '.height', 100,
            '.x', 10,
            '.y', 10,
            '.style', 'background: red',
            '.icon', '',
            '.label', 'TEST',
            'text', 'DEFAULT',
        )
    )";
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT + script, "DEFAULT\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QLineEdit in :QDialog" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "dialog('.title', 'Remove Items', '.label', 'Remove all items?') === true", "true\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QPushButton in dialog_Remove Items:QDialog" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    const QByteArray script2 = R"(
        dialog(
            '.modal', true,
            '.onTop', true,
            'text', 'DEFAULT',
        )
    )";
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT + script2, "DEFAULT\n"); },
        [&]() { RUN(Args() << "keys" << "focus::QLineEdit in :QDialog" << "ENTER", ""); }
    );
}

void Tests::commandDialogCloseOnDisconnect()
{
    RUN("afterMilliseconds(0, abort); dialog()", "");
}

void Tests::commandMenuItems()
{
    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems('a', 'b', 'c')", "a\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems([{'text/plain': 'a'}, {'text/plain': 'b'}])", "0\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems('a', 'b', 'c')", "\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << "ESCAPE", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems([{'text/plain': 'a'}, {'text/plain': 'b'}])", "-1\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << "ESCAPE", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems('a', 'b', 'c')", "b\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << ":b" << "ENTER", ""); }
    );

    RUN(Args() << "keys" << clipboardBrowserId, "");
    runMultiple(
        [&]() { RUN(WITH_TIMEOUT "menuItems([{'text/plain': 'a'}, {'text/plain': 'b'}])", "1\n"); },
        [&]() { RUN(Args() << "keys" << customMenuId << ":b" << "ENTER", ""); }
    );

    RUN("afterMilliseconds(0, abort); menuItems('a', 'b', 'c')", "");
}

void Tests::commandsPackUnpack()
{
    QMap<QLatin1String, QByteArray> data;
    data[mimeText] = "plain text";
    data[mimeHtml] = "<b>HTML text</b>";
    data[QLatin1String(COPYQ_MIME_PREFIX "test1")] = "test1 data";
    data[QLatin1String(COPYQ_MIME_PREFIX "test2")] = "test2 data";

    auto args = Args() << "write";
    for (const auto &mime : data.keys())
        args << mime << data[mime];
    RUN(args, "");

    const QByteArray script1 =
            "var data = read('" + toByteArray(mimeItems) + "', 0); var item = unpack(data);";

    // Unpack item read from list.
    for (const auto &mime : data.keys()) {
        RUN("eval"
            << script1 + "var mime = '" + mime + "'; print(mime + ':' + str(item[mime]))",
            mime + ':' + data[mime]);
    }

    // Test pack and unpack consistency.
    const QByteArray script2 = "data = pack(item); item = unpack(data);";
    for (const auto &mime : data.keys()) {
        RUN("eval"
            << script1 + script2 + "var mime = '" + mime + "'; print(mime + ':' + str(item[mime]))",
            mime + ':' + data[mime]);
    }
}

void Tests::commandsBase64()
{
    const QByteArray data = "0123456789\001\002\003\004\005\006\007abcdefghijklmnopqrstuvwxyz!";
    const QByteArray base64 = data.toBase64();

    TEST( m_test->runClient(Args() << "eval" << "print(input())", data, data) );
    TEST( m_test->runClient(Args() << "eval" << "print(tobase64(input()))", base64, data) );

    // Line break is added only if return value is string;
    // tobase64() returns string, frombase64() returns byte array.
    RUN("tobase64" << data, base64 + '\n');
    RUN("frombase64" << base64, data);

    TEST( m_test->runClient(Args() << "eval" << "print(frombase64(tobase64(input())))", data, data) );

    // Test Base64 encoding and decoding consistency.
    TEST( m_test->runClient(
              Args() << "eval"
              << "var base64 = tobase64(input()); if (str(input()) === str(frombase64(base64))) print('OK')",
              "OK", data) );
}

void Tests::commandsGetSetItem()
{
    QMap<QByteArray, QByteArray> data;
    data["text/plain"] = "plain text";
    data["text/html"] = "<b>HTML text</b>";
    data[COPYQ_MIME_PREFIX "test1"] = "test1 data";
    data[COPYQ_MIME_PREFIX "test2"] = "test2 data";

    const QString tab = testTab(1);
    const Args args = Args("tab") << tab;

    Args args2 = args;
    args2 << "write";
    for (const auto &mime : data.keys())
        args2 << mime << data[mime];
    RUN(args2, "");

    // Get item from list.
    for (const auto &mime : data.keys()) {
        RUN(args << "eval"
            << "var mime = '" + mime + "'; print(mime + ':' + str(getitem(0)[mime]))",
            mime + ':' + data[mime]);
    }

    // Set item.
    RUN(args << "eval"
        << "setitem(1, { 'text/plain': 'plain text 2', 'text/html': '<b>HTML text 2</b>' })",
        "");

    RUN(args << "eval" << "print(getitem(1)['text/plain'])", "plain text 2");
    RUN(args << "eval" << "print(getitem(1)['text/html'])", "<b>HTML text 2</b>");
}

void Tests::commandsChecksums()
{
    RUN("md5sum" << "TEST", "033bd94b1168d7e4f0d644c3c95e35bf\n");
    RUN("sha1sum" << "TEST", "984816fd329622876e14907634264e6f332e9fb3\n");
    RUN("sha256sum" << "TEST", "94ee059335e587e501cc4bf90613e0814f00a7b08bc7c648fd865a2af6a22cc2\n");
    RUN("sha512sum" << "TEST", "7bfa95a688924c47c7d22381f20cc926f524beacb13f84e203d4bd8cb6ba2fce81c57a5f059bf3d509926487bde925b3bcee0635e4f7baeba054e5dba696b2bf\n");
}

void Tests::commandEscapeHTML()
{
    RUN("escapeHTML" << "&\n<\n>", "&amp;<br />&lt;<br />&gt;\n");
}

void Tests::commandExecute()
{
    const QByteArray script =
            "function test(c, expected_stdout, expected_exit_code) {"
            "    if (str(c.stdout) !== expected_stdout) print('Unexpected stdout: \"' + str(c.stdout) + '\"');"
            "    if (c.exit_code !== expected_exit_code) print('Unexpected exit_code: ' + str(c.exit_code));"
            "}";

    RUN("eval" << script +
        "c = execute('copyq', 'write', 'text/plain', 'plain text', 'text/html', '<b>test HTML</b>');"
        "test(c, '', 0);"
        , "");

    RUN("eval" << script +
        "c = execute('copyq', 'read', 'text/plain', 0);"
        "test(c, 'plain text', 0);"
        , "");

    RUN("eval" << script +
        "c = execute('copyq', 'read', 'text/html', 0);"
        "test(c, '<b>test HTML</b>', 0);"
        , "");

    RUN("eval" << script +
        "c = execute('copyq', 'read', 0, function(lines) { print(lines); });"
        "test(c, 'plain text', 0);"
        , "plain text");
}

void Tests::commandSettings()
{
    RUN("config" << "clipboard_tab" << "TEST", "TEST\n");

    RUN("settings" << "test_variable", "");
    RUN("settings" << "test_variable" << "TEST VALUE", "");
    RUN("settings" << "test_variable", "TEST VALUE");
    RUN("settings" << "test_variable" << "TEST VALUE 2", "");
    RUN("settings" << "test_variable", "TEST VALUE 2");

    RUN("config" << "clipboard_tab", "TEST\n");
}

void Tests::commandsEnvSetEnv()
{
    RUN("eval" <<
        "\n var name = 'COPYQ_ENV_TEST'"
        "\n if (setEnv(name, 'OK'))"
        "\n   print(env(name))"
        "\n else"
        "\n   print('FAILED')"
        , "OK"
        );
}

void Tests::commandSleep()
{
    QElapsedTimer t;

    t.start();
    RUN("sleep" << "100", "");
    const auto afterElapsed100Ms = t.elapsed();
    QVERIFY(afterElapsed100Ms > 100);

    t.start();
    RUN("sleep" << "1000", "");
    const auto afterElapsed1000Ms = t.elapsed();
    QVERIFY(afterElapsed1000Ms > 1000);
}

void Tests::commandsData()
{
    RUN("eval" << "setData('x', 'X'); data('x')", "X");
    RUN("eval" << "setData('x', 'X'); setData('y', 'Y'); str(data('x')) + str(data('y'))", "XY\n");

    RUN("dataFormats", "");
    RUN("eval" << "setData('x'); dataFormats()", "x\n");
    RUN("eval" << "setData('x'); setData('y'); dataFormats()", "x\ny\n");

    RUN("eval" << "setData('x'); setData('y'); removeData('x'); dataFormats()", "y\n");
    RUN("eval" << "setData('x'); setData('y'); removeData('y'); dataFormats()", "x\n");
}

void Tests::commandCurrentWindowTitle()
{
    RUN("disable", "");
    WAIT_ON_OUTPUT("currentWindowTitle", appWindowTitle("*Clipboard Storing Disabled*"));
    RUN("enable", "");
}

void Tests::commandCopy()
{
    RUN("copy" << "A", "true\n");
    WAIT_FOR_CLIPBOARD("A");

    RUN("copy" << "DATA" << "B", "true\n");
    WAIT_FOR_CLIPBOARD2("B", "DATA");

    RUN( Args() << "copy"
         << "DATA3" << "C"
         << "DATA4" << "D"
         , "true\n" );
    WAIT_FOR_CLIPBOARD2("C", "DATA3");
    WAIT_FOR_CLIPBOARD2("D", "DATA4");

    RUN( "copy({'DATA1': 1, 'DATA2': 2})", "true\n" );
    WAIT_FOR_CLIPBOARD2("1", "DATA1");
    WAIT_FOR_CLIPBOARD2("2", "DATA2");

    RUN_EXPECT_ERROR_WITH_STDERR(
        "copy({}, {})",
        CommandException, "Expected single item");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "copy([{}, {}])",
        CommandException, "Expected single item");
}

void Tests::commandClipboard()
{
    TEST( m_test->setClipboard("A") );
    WAIT_FOR_CLIPBOARD("A");
    RUN("clipboard", "A");

    TEST( m_test->setClipboard("B", "DATA") );
    WAIT_FOR_CLIPBOARD2("B", "DATA");
    RUN("clipboard" << "DATA", "B");
}

void Tests::commandHasClipboardFormat()
{
    TEST( m_test->setClipboard("B", "DATA") );
    WAIT_FOR_CLIPBOARD2("B", "DATA");
    WAIT_ON_OUTPUT("hasClipboardFormat('DATA')", "true\n");
    WAIT_ON_OUTPUT("hasClipboardFormat('text/plain')", "false\n");
}

void Tests::commandEdit()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_COMMAND_EDIT");

    RUN("config" << "editor" << "", "\n");

    // Edit clipboard and new item.
    TEST( m_test->setClipboard("TEST") );
    RUN("edit" << "-1", "");
    RUN("keys" << "END" << ":LINE 1" << "F2", "");
    RUN("read" << "0", "TESTLINE 1");
    WAIT_FOR_CLIPBOARD("TESTLINE 1");

    // Edit existing item.
    RUN("edit" << "0", "");
    RUN("keys" << "END" << "ENTER" << ":LINE 2" << "F2", "");
    RUN("read" << "0", "TESTLINE 1\nLINE 2");
    WAIT_FOR_CLIPBOARD("TESTLINE 1");

    // Edit clipboard (ignore existing data) and new item.
    RUN("edit", "");
    RUN("keys" << "END" << ":LINE 1" << "F2", "");
    RUN("read" << "0", "LINE 1");
    WAIT_FOR_CLIPBOARD("LINE 1");
}

void Tests::commandEditItem()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_COMMAND_EDIT");

    RUN("config" << "editor" << "", "\n");

    // Edit clipboard and new item.
    TEST( m_test->setClipboard("TEST", mimeHtml) );
    RUN("editItem" << "-1" << mimeHtml, "");
    RUN("keys" << "END" << ":LINE 1" << "F2", "");
    const auto expected = QByteArrayLiteral("TESTLINE 1");
#ifdef Q_OS_WIN
    const auto expectedClipboard = QByteArrayLiteral("<!--StartFragment-->TESTLINE 1<!--EndFragment-->", mimeHtml);
#else
    const auto expectedClipboard = expected;
#endif
    RUN("read" << mimeHtml << "0", expected);
    RUN("read" << "0", "");
    WAIT_FOR_CLIPBOARD2(expectedClipboard, mimeHtml);
    WAIT_FOR_CLIPBOARD("");

    // Edit existing item.
    RUN("editItem" << "0" << mimeHtml, "");
    RUN("keys" << "END" << "ENTER" << ":LINE 2" << "F2", "");
    RUN("read" << mimeHtml << "0", expected + "\nLINE 2");
    RUN("read" << "0", "");
    WAIT_FOR_CLIPBOARD2(expectedClipboard, mimeHtml);
    WAIT_FOR_CLIPBOARD("");

    // Edit clipboard (ignore existing data) and new item.
    RUN("editItem" << "-1" << mimeHtml << "TEST", "");
    RUN("keys" << "END" << ":LINE 1" << "F2", "");
    RUN("read" << mimeHtml << "0", "TESTLINE 1");
    RUN("read" << "0", "");
    WAIT_FOR_CLIPBOARD2(expectedClipboard, mimeHtml);
    WAIT_FOR_CLIPBOARD("");
}

void Tests::commandGetSetCurrentPath()
{
    RUN("currentPath", QDir::currentPath() + "\n");

    const auto newPath = QDir::homePath().toUtf8();

    TEST( m_test->runClient(
              Args("eval") << "currentPath(input()); print(currentPath())",
              newPath, newPath) );

    TEST( m_test->runClient(
              Args("eval") << "currentPath(input()); print(Dir().absolutePath())",
              newPath, newPath) );
}

void Tests::commandSelectItems()
{
    RUN("add" << "C" << "B" << "A", "");

    RUN("selectItems" << "1", "true\n");
    RUN("testSelected", QString(clipboardTabName) + " 1 1\n");

    RUN("selectItems" << "0" << "2", "true\n");
    RUN("testSelected", QString(clipboardTabName) + " 2 0 2\n");

    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;
    RUN(args << "add" << "C" << "B" << "A", "");
    RUN(args << "selectItems" << "1" << "2", "true\n");
    RUN("testSelected", QString(clipboardTabName) + " 2 0 2\n");
    RUN("setCurrentTab" << tab, "");
    RUN("testSelected", tab + " 2 1 2\n");
}

void Tests::commandsExportImport()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "C" << "B" << "A", "");

    const auto tab2 = testTab(2);
    RUN("tab" << tab2 << "add" << "3" << "2", "");

    RUN("config" << "maxitems" << "3", "3\n");
    RUN("config" << "editor" << "EDITOR1 %1", "EDITOR1 %1\n");

    TemporaryFile tmp;
    const auto fileName = tmp.fileName();

    RUN("exportData" << fileName, "");

    RUN("config" << "maxitems" << "1", "1\n");
    RUN("config" << "editor" << "EDITOR2 %1", "EDITOR2 %1\n");
    RUN("removetab" << tab1, "");
    RUN("tab" << tab2 << "add" << "1", "");

    RUN("importData" << fileName, "");

    RUN("config" << "maxitems", "3\n");
    RUN("config" << "editor", "EDITOR1 %1\n");

    const auto suffix = " (1)";
    RUN("tab",
        QString(clipboardTabName) + "\n"
        + tab2 + "\n"
        + clipboardTabName + suffix + "\n"
        + tab1 + "\n"
        + tab2 + suffix + "\n");

    RUN("tab" << tab1 << "read" << "0" << "1" << "2", "A\nB\nC");
    RUN("tab" << tab2 + suffix << "read" << "0" << "1", "2\n3");
    RUN("tab" << tab2 << "read" << "0", "1");
}

void Tests::commandsGetSetCommands()
{
    RUN("commands().length", "0\n");

    RUN("setCommands([{name: 'test', cmd: 'copyq help'}])", "");
    RUN("commands().length", "1\n");
    RUN("commands()[0].name", "test\n");
    RUN("commands()[0].cmd", "copyq help\n");
    RUN("commands()[0].enable", "true\n");

    RUN("setCommands(commands())", "");
    RUN("commands().length", "1\n");
    RUN("commands()[0].name", "test\n");
    RUN("commands()[0].enable", "true\n");
}

void Tests::commandsImportExportCommands()
{
   const QString commands =
           R"('
           [Commands]
           1\Name=Test 1
           2\Name=Test 2
           size=2
           ')";
   RUN("eval" << "importCommands(arguments[1]).length" << "--" << commands, "2\n");
   RUN("eval" << "importCommands(arguments[1])[0].name" << "--" << commands, "Test 1\n");
   RUN("eval" << "importCommands(arguments[1])[1].name" << "--" << commands, "Test 2\n");

   RUN("importCommands(exportCommands([{},{}])).length", "2\n");
   RUN("importCommands(exportCommands([{},{name: 'Test 2'}]))[1].name", "Test 2\n");
}

void Tests::commandsImportExportCommandsFixIndentation()
{
    {
        const QString commands =
                "[Command]\n"
                "Command=\"\n    1\n    2\n    3\"";
        RUN("eval" << "importCommands(arguments[1])[0].cmd" << "--" << commands, "1\n2\n3\n");
    }

    {
        const QString commands =
                "[Command]\n"
                "Command=\"\r\n    1\r\n    2\r\n    3\"";
        RUN("eval" << "importCommands(arguments[1])[0].cmd" << "--" << commands, "1\n2\n3\n");
    }
}

void Tests::commandsAddCommandsRegExp()
{
    const QString commands =
            "[Command]\n"
            "Match=^(https?|ftps?)://\\\\$\n";

    // Ensure there is a basic RegExp support.
    RUN("/test/", "/test/\n");
    RUN("/test/.source", "test\n");

    RUN("eval" << "exportCommands(importCommands(arguments[1]))" << "--" << commands, commands);
    RUN("eval" << "Object.prototype.toString.call(importCommands(arguments[1])[0].re)" << "--" << commands, "[object RegExp]\n");
    RUN("eval" << "Object.prototype.toString.call(importCommands(arguments[1])[0].wndre)" << "--" << commands, "[object RegExp]\n");
    RUN("eval" << "importCommands(arguments[1])[0].re" << "--" << commands, "/^(https?|ftps?):\\/\\/\\$/\n");
    RUN("eval" << "importCommands(arguments[1])[0].wndre" << "--" << commands, "/(?:)/\n");

    RUN("eval" << "addCommands(importCommands(arguments[1]))" << "--" << commands, "");
    RUN("keys" << commandDialogListId << "Enter" << clipboardBrowserId, "");

    RUN("exportCommands(commands())", commands);
    RUN("commands()[0].name", "\n");
    RUN("commands()[0].re", "/^(https?|ftps?):\\/\\/\\$/\n");
    RUN("commands()[0].wndre", "/(?:)/\n");
}

void Tests::commandScreenshot()
{
    RUN("screenshot().size() > 0", "true\n");
}

void Tests::commandNotification()
{
    const auto script = R"(
        notification(
            '.title', 'title',
            '.message', 'message',
            '.time', 1000,
            '.id', 'test',
            '.icon', 'copyq',
            '.button', 'OK', '', '',
            '.button', 'CANCEL', '', '',
            '.urgency', 'critical',
            '.persistent', true,
        )
        )";
    RUN(script, "");

    RUN_EXPECT_ERROR_WITH_STDERR(
                "notification('.message', 'message', 'BAD')", CommandException, "Unknown argument: BAD");
}

void Tests::commandNotificationUrgency()
{
    RUN("notification('.urgency', 'low')", "");
    RUN("notification('.urgency', 'normal')", "");
    RUN("notification('.urgency', 'high')", "");
    RUN("notification('.urgency', 'critical')", "");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "notification('.urgency', 'unknown')",
        CommandException,
        "Unknown value for '.urgency' notification field: unknown");
}

void Tests::commandNotificationPersistent()
{
    RUN("notification('.persistent', true)", "");
    RUN("notification('.persistent', false)", "");
    RUN("notification('.persistent', 1)", "");
    RUN("notification('.persistent', 0)", "");
    RUN("notification('.persistent', 'true')", "");
    RUN("notification('.persistent', 'false')", "");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "notification('.persistent', 'unknown')",
        CommandException,
        "Unknown value for '.persistent' notification field: unknown");
}

void Tests::commandIcon()
{
    RUN("iconColor", QByteArray(defaultSessionColor) + "\n");

    RUN("iconColor" << "red", "");
    RUN("iconColor", "#ff0000\n");

    RUN_EXPECT_ERROR("iconColor" << "BAD_COLOR_NAME", CommandException);
    RUN("iconColor", "#ff0000\n");

    RUN("iconColor" << "", "");
    RUN("iconColor", QByteArray(defaultSessionColor) + "\n");

    RUN("iconColor" << defaultSessionColor, "");
    RUN("iconColor", QByteArray(defaultSessionColor) + "\n");
}

void Tests::commandIconTag()
{
    RUN("iconTag", "\n");

    RUN("iconTag" << "TEST", "");
    RUN("iconTag", "TEST\n");

    RUN("iconTag" << "", "");
    RUN("iconTag", "\n");
}

void Tests::commandIconTagColor()
{
    RUN("iconTagColor", QByteArray(defaultTagColor) + "\n");

    RUN("iconTagColor" << "red", "");
    RUN("iconTagColor", "#ff0000\n");

    RUN_EXPECT_ERROR("iconTagColor" << "BAD_COLOR_NAME", CommandException);
    RUN("iconTagColor", "#ff0000\n");

    RUN("iconTagColor" << defaultTagColor, "");
    RUN("iconTagColor", QByteArray(defaultTagColor) + "\n");
}

void Tests::commandLoadTheme()
{
    RUN_EXPECT_ERROR_WITH_STDERR(
        "loadTheme" << "a non-existent file", CommandException, "ScriptError: Failed to read theme");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "loadTheme" << ".", CommandException, "ScriptError: Failed to read theme");

    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.write("INVALID INI FILE");
        tmp.close();
        RUN_EXPECT_ERROR_WITH_STDERR(
            "loadTheme" << tmp.fileName(), CommandException, "ScriptError: Failed to parse theme");
    }

    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.write("[General]");
        tmp.close();
        RUN("loadTheme" << tmp.fileName(), "");
    }

    // Verify default stylesheets - if there is a syntax error,
    // application prints a warning which should be captured by tests.
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.write("[General]\nstyle_main_window=true");
        tmp.close();
        RUN("loadTheme" << tmp.fileName(), "");
    }
}

void Tests::commandDateString()
{
    const auto dateFormat = "TEST:yyyy-MM-dd";
    const auto dateTime = QDateTime::currentDateTime();
    const auto today = dateTime.toString(dateFormat);
    RUN("dateString" << dateFormat, today + "\n");
}

void Tests::commandAfterMilliseconds()
{
    const QString script = "afterMilliseconds(100, function(){ print('TEST'); abort(); });";
    RUN(script, "");
    RUN(script + "sleep(1)", "");
    RUN(script + "sleep(200)", "TEST");
}

void Tests::commandAsync()
{
    RUN("afterMilliseconds(0, function() { print(currentItem()); abort(); }); dialog()", "-1");
}

void Tests::commandFilter()
{
    RUN("filter", "\n");
    RUN("filter" << "test", "");
    RUN("filter", "test\n");
    RUN("filter" << "another", "");
    RUN("filter", "another\n");
    RUN("filter" << "", "");
    RUN("filter", "\n");

    // Empty filter() after ESC.
    RUN("filter" << "test", "");
    RUN("filter", "test\n");
    RUN("keys" << "ESC", "");
    RUN("filter", "\n");
}

void Tests::commandMimeTypes()
{
    RUN("print(mimeText)", mimeText);
    RUN("print(mimeHtml)", mimeHtml);
    RUN("print(mimeUriList)", mimeUriList);
    RUN("print(mimeWindowTitle)", mimeWindowTitle);
    RUN("print(mimeItems)", mimeItems);
    RUN("print(mimeItemNotes)", mimeItemNotes);
    RUN("print(mimeOwner)", mimeOwner);
    RUN("print(mimeClipboardMode)", mimeClipboardMode);
    RUN("print(mimeCurrentTab)", mimeCurrentTab);
    RUN("print(mimeSelectedItems)", mimeSelectedItems);
    RUN("print(mimeCurrentItem)", mimeCurrentItem);
    RUN("print(mimeHidden)", mimeHidden);
    RUN("print(mimeShortcut)", mimeShortcut);
    RUN("print(mimeColor)", mimeColor);
    RUN("print(mimeOutputTab)", mimeOutputTab);
}

void Tests::commandUnload()
{
    // Failure if tab is visible.
    RUN("unload", "");

    const auto tab = testTab(1);

    // Success if tab doesn't exist.
    RUN("unload" << tab, tab + "\n");

    RUN("tab" << tab << "add" << "A", "");
    // Success if tab is not visible and editor is not open.
    RUN("unload" << tab, tab + "\n");

    RUN("tab" << tab << "add" << "B", "");
    RUN("unload", tab + "\n");
    // Success if tab is not loaded.
    RUN("unload", tab + "\n");

    // Success if tab does not exist.
    RUN("unload" << "missing-tab", "missing-tab\n");
}

void Tests::commandForceUnload()
{
    RUN("forceUnload", "");
    RUN_EXPECT_ERROR_WITH_STDERR("add" << "A", CommandException, "ScriptError: Invalid tab");

    RUN("keys" << clipboardBrowserRefreshButtonId << "Space", "");
    RUN("add" << "A", "");

    const auto tab = testTab(1);
    RUN("tab" << tab << "add" << "A", "");

    RUN("forceUnload" << tab, "");

    RUN("setCurrentTab" << tab, "");
    RUN_EXPECT_ERROR_WITH_STDERR(
        "tab" << tab << "add" << "B", CommandException, "ScriptError: Invalid tab");

    RUN("keys" << clipboardBrowserRefreshButtonId << "Space", "");
    RUN("add" << "B", "");
}

void Tests::commandServerLogAndLogs()
{
    const QByteArray data1 = generateData();
    QRegularExpression re("CopyQ Note \\[[^]]+\\] <Server-[0-9]+>: " + QRegularExpression::escape(data1));

    QByteArray stdoutActual;
    QByteArray stderrActual;

    QCOMPARE( run(Args("logs"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );
    QVERIFY( !QString::fromUtf8(stdoutActual).contains(re) );

    RUN("serverLog" << data1, "");

    QCOMPARE( run(Args("logs"), &stdoutActual, &stderrActual), 0 );
    QVERIFY2( testStderr(stderrActual), stderrActual );
    QVERIFY( !stdoutActual.isEmpty() );
    QVERIFY( QString::fromUtf8(stdoutActual).contains(re) );
}

void Tests::chainingCommands()
{
    const auto tab1 = testTab(1);
    RUN("tab" << tab1 << "add" << "C" << "B" << "A", "");
    RUN("tab" << tab1 << "separator" << " " << "read" << "0" << "1" << "2", "A B C");
    RUN("tab" << tab1 << "separator" << "\\t" << "showAt" << "read" << "0" << "1" << "2", "A\tB\tC");

    // Chain functions without arguments.
    RUN("enable" << "disable" << "monitoring", "false\n");
    RUN("if (!monitoring()) enable" << "monitoring", "true\n");

    // Don't treat arguments after "eval" as functions.
    RUN("eval" << "arguments[1]" << "TEST", "TEST");
    RUN("eval" << "arguments[1]" << "--" << "TEST", "TEST");
}

void Tests::insertRemoveItems()
{
    const Args args = Args("tab") << testTab(1) << "separator" << ",";

    RUN(args << "add" << "ghi" << "abc", "");
    RUN(args << "insert" << "1" << "def", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "abc,def,ghi,,");

    RUN(args << "insert" << "0" << "012", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "012,abc,def,ghi,");

    RUN(args << "remove" << "0" << "2", "");
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "abc,ghi,,,");

    QByteArray in("ABC");
    QCOMPARE( run(Args(args) << "insert" << "1" << "-", nullptr, nullptr, in), 0);
    RUN(args << "read" << "0" << "1" << "2" << "3" << "4", "abc,ABC,ghi,,");
}

void Tests::handleUnexpectedTypes()
{
    RUN("add(version)", "");
    RUN("add([version])", "");
    RUN("add({version: version})", "");
}

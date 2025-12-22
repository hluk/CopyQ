// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

#include <QRegularExpression>
#include <QStandardPaths>

void Tests::tabEncryption()
{
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    // Enable tab encryption - this should work even if the encryption support
    // is not build into the app.
    RUN("config" << "encrypt_tabs" << "true", "true\n");

    // Verify tab doesn't exist yet
    QVERIFY( !hasTab(tab) );

    // Add test data to the tab
    RUN(args << "size", "0\n");
    RUN(args << "add" << "test data 3" << "test data 2" << "test data 1", "");
    QVERIFY( hasTab(tab) );

    // Verify data was added correctly
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "test data 1 test data 2 test data 3");

    // Verify tab reloads properly
    RUN("unload" << tab, tab + "\n");
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "test data 1 test data 2 test data 3");

    // Restart server to verify encryption/decryption works
    // The COPYQ_PASSWORD environment variable should be used automatically
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );
    RUN("show", "");
    KEYS(clipboardBrowserId);

    // Verify tab still exists
    QVERIFY( hasTab(tab) );

    // Verify data persisted and was decrypted correctly
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "test data 1 test data 2 test data 3");

#ifdef WITH_QCA_ENCRYPTION
    // Password is needed when disabling encryption
    runMultiple(
        [&]() { KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); },
        [&]() { RUN("config" << "encrypt_tabs" << "false", "false\n"); }
    );
    KEYS(clipboardBrowserId);
#endif

    // Verify that password is not needed after disabling encryption
    m_test->setEnv("COPYQ_PASSWORD", "");
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "test data 1 test data 2 test data 3");
}

void Tests::tabEncryptionPasswordNew()
{
#ifdef WITH_QCA_ENCRYPTION
    m_test->setEnv("COPYQ_PASSWORD", "");
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );
    RUN("show", "");
    KEYS(clipboardBrowserId);

    runMultiple(
        [&]() {
            KEYS(
                // Choose new password
                passwordEntryNewId << ":TEST123" << "ENTER"
                // Retype the password
                << passwordEntryRetypeId << ":TEST123" << "ENTER"
            );
        },
        [&]() { RUN("config" << "encrypt_tabs" << "true", "true\n"); }
    );
    KEYS(clipboardBrowserId);

    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";

    // Add test data to the tab
    RUN(args << "size", "0\n");
    RUN(args << "add" << "test data 3" << "test data 2" << "test data 1", "");
    QVERIFY( hasTab(tab) );

    // Verify data was added correctly
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "test data 1 test data 2 test data 3");

    // Verify tab reloads properly
    RUN("unload" << tab, tab + "\n");
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "test data 1 test data 2 test data 3");
#else
    SKIP("Encryption support not built-in");
#endif
}

void Tests::tabEncryptionPasswordCurrent()
{
#ifdef WITH_QCA_ENCRYPTION
    const QString tab = testTab(1);
    const Args args = Args("tab") << tab << "separator" << " ";
    RUN(args << "add" << "test data 3" << "test data 2" << "test data 1", "");

    RUN("config" << "encrypt_tabs" << "true", "true\n");

    m_test->setEnv("COPYQ_PASSWORD", ":TEST");
    m_test->ignoreErrors(QRegularExpression("Loaded password does not match the stored hash"));
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );

    KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER");

    WAIT_ON_OUTPUT(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2", "test data 1 test data 2 test data 3");
#else
    SKIP("Encryption support not built-in");
#endif
}

void Tests::tabEncryptionPasswordRetry()
{
#ifdef WITH_QCA_ENCRYPTION
    m_test->setEnv("COPYQ_PASSWORD", "");
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );
    RUN("show", "");
    KEYS(clipboardBrowserId);

    runMultiple(
        [&]() {
            KEYS(
                // Choose new password - attempt 1
                passwordEntryNewId << ":TEST123" << "ENTER"
                // Retype the password - attempt 1
                << passwordEntryRetypeId << ":TEST1234" << "ENTER"
                // Choose new password - attempt 2
                << passwordEntryNewId << ":TEST123" << "ENTER"
                // Retype the password - attempt 2
                << passwordEntryRetypeId << ":TEST123" << "ENTER"
            );
        },
        [&]() { RUN("config" << "encrypt_tabs" << "true", "true\n"); }
    );
    KEYS(clipboardBrowserId);
#else
    SKIP("Encryption support not built-in");
#endif
}

void Tests::tabEncryptionPasswordRetryFail()
{
#ifdef WITH_QCA_ENCRYPTION
    m_test->setEnv("COPYQ_PASSWORD", "");
    TEST( m_test->stopServer() );
    TEST( m_test->startServer() );
    RUN("show", "");
    KEYS(clipboardBrowserId);

    runMultiple(
        [&]() {
            KEYS(
                // Choose new password - attempt 1
                passwordEntryNewId << ":TEST123" << "ENTER"
                // Retype the password - attempt 1
                << passwordEntryRetypeId << ":TEST1234" << "ENTER"

                // Choose new password - attempt 2
                << passwordEntryNewId << "ENTER"
                // Empty password - attempt 2
                << passwordMessageEmptyId << "ENTER"

                // Choose new password - attempt 3
                << passwordEntryNewId << ":TEST123" << "ENTER"
                // Retype the password - attempt 3
                << passwordEntryRetypeId << ":TEST1234" << "ENTER"

                << passwordMessageFailedId << "ENTER"
            );
        },
        [&]() { RUN("config" << "encrypt_tabs" << "true", "true\n"); }
    );

    // If the initial password was not provided, encryption should be disabled.
    RUN("config" << "encrypt_tabs", "false\n");
#else
    SKIP("Encryption support not built-in");
#endif
}

void Tests::tabEncryptionLargeItems()
{
#ifdef WITH_QCA_ENCRYPTION
    RUN("config" << "encrypt_tabs" << "true", "true\n");

    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;

    const auto script = R"(
        write(0, [{
            'text/plain': '1234567890'.repeat(10000),
            'application/x-copyq-test-data': 'abcdefghijklmnopqrstuvwxyz'.repeat(10000),
        }])
        )";
    RUN(args << script, "");

    RUN(args << "read(0).left(20)", "12345678901234567890");
    RUN(args << "read(0).length", "100000\n");
    RUN(args << "ItemSelection().selectAll().itemAtIndex(0)[mimeText].length", "100000\n");

    RUN("unload" << tab, tab + "\n");
    RUN("show" << tab, "");

    // Read data after reloading the tab
    RUN(args << "read(0).left(20)", "12345678901234567890");
    RUN(args << "read(0).length", "100000\n");
    RUN(args << "ItemSelection().selectAll().itemAtIndex(0)[mimeText].length", "100000\n");
#else
    SKIP("Encryption support not built-in");
#endif
}

void Tests::tabEncryptionChangePassword()
{
#ifdef WITH_QCA_ENCRYPTION
    RUN("config" << "encrypt_tabs" << "true", "true\n");

    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;
    RUN("show" << tab, "");

    RUN(args << "add" << "3" << "2" << "1", "");
    QVERIFY( hasTab(tab) );
    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");

    // Open preferences dialog.
    KEYS("Ctrl+P" << configurationDialogId);

    KEYS("mouse|PRESS|pushButtonChangeEncryptionPassword");
    KEYS("mouse|RELEASE|pushButtonChangeEncryptionPassword");
    KEYS(
        // Current password is always needed when changing
        passwordEntryCurrentId << ":TEST123" << "ENTER"

        << passwordEntryNewId << ":TEST1234" << "ENTER"
        << passwordEntryRetypeId << ":TEST1234" << "ENTER"
        << passwordMessageChangeId << "ENTER"
    );

    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");

    TEST( m_test->stopServer() );
    m_test->setEnv("COPYQ_PASSWORD", "TEST1234");
    TEST( m_test->startServer() );

    RUN(args << "size", "3\n");
    RUN(args << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");
#else
    SKIP("Encryption support not built-in");
#endif
}

void Tests::tabEncryptionMissingHash()
{
    // Ensure that missing hash file does not lock users out from their data.
#ifdef WITH_QCA_ENCRYPTION
    RUN("config" << "encrypt_tabs" << "true", "true\n");

    const auto tab = testTab(1);
    const auto args = Args("tab") << tab;
    RUN("show" << tab, "");

    RUN(args << "add" << "3" << "2" << "1", "");
    RUN(args << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");

    TEST( m_test->stopServer() );

    // Remove hash file
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY2( QFile::remove(path + "/.keydata"), "Hash file should exist" );
    m_test->ignoreErrors(QRegularExpression("Hash is missing, accepting any password"));

    TEST( m_test->startServer() );
    RUN("show" << tab, "");
    KEYS(clipboardBrowserId);
    RUN(args << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");

    TEST( m_test->stopServer() );

    // Set wrong password
    m_test->setEnv("COPYQ_PASSWORD", "TEST1234");
    m_test->ignoreErrors(QRegularExpression(
        "Hash is missing, accepting any password"
        "|Unwrap DEK: finalization failed"
        "|Cannot decrypt data .* no valid encryption key provided"
    ));

    TEST( m_test->startServer() );
    RUN("show" << "", "");
    KEYS(clipboardBrowserId);
    RUN(args << "size", "0\n");
    RUN_EXPECT_ERROR_WITH_STDERR(
        args << "add" << "TEST", 4, "ScriptError: Invalid tab");

    // Try to disable decryption with a wrong password
    runMultiple(
        [&]() { KEYS(passwordEntryCurrentId << ":TEST1234" << "ENTER"); },
        [&]() { RUN("config" << "encrypt_tabs" << "false", "false\n"); }
    );
    KEYS(clipboardBrowserId);
    RUN("config" << "encrypt_tabs", "true\n");

    TEST( m_test->stopServer() );

    m_test->setEnv("COPYQ_PASSWORD", "TEST123");

    TEST( m_test->startServer() );
    RUN("show" << tab, "");
    KEYS(clipboardBrowserId);
    RUN(args << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");

    // Disable decryption
    runMultiple(
        [&]() { KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); },
        [&]() { RUN("config" << "encrypt_tabs" << "false", "false\n"); }
    );
    KEYS(clipboardBrowserId);
    RUN("config" << "encrypt_tabs", "false\n");

    TEST( m_test->stopServer() );

    m_test->setEnv("COPYQ_PASSWORD", "");
    m_test->ignoreErrors({});
    QVERIFY2( dropLogsToFileCountAndSize(0, 0), "Failed to remove log files" );

    TEST( m_test->startServer() );
    RUN("show" << tab, "");
    KEYS(clipboardBrowserId);
    RUN(args << "read" << "0" << "1" << "2" << "3", "1\n2\n3\n");
#else
    SKIP("Encryption support not built-in");
#endif
}

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

#include "itemencryptedtests.h"

#include "tests/test_utils.h"

ItemEncryptedTests::ItemEncryptedTests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

void ItemEncryptedTests::initTestCase()
{
    SKIP_ON_ENV("COPYQ_TESTS_SKIP_ITEMENCRYPT");

    TEST(m_test->initTestCase());
}

void ItemEncryptedTests::cleanupTestCase()
{
    TEST(m_test->cleanupTestCase());
}

void ItemEncryptedTests::init()
{
    TEST(m_test->init());
}

void ItemEncryptedTests::cleanup()
{
    TEST( m_test->cleanup() );
}

void ItemEncryptedTests::encryptDecryptData()
{
    if ( !isGpgInstalled() )
        SKIP("gpg2 is required to run the test");

    RUN("-e" << "plugins.itemencrypted.generateTestKeys()", "\n");

    // Test gpg errors first.
    RUN("-e" << "plugins.itemencrypted.encrypt(input());print('')", "");

    const QByteArray input("\x00\x01\x02\x03\x04", 5);
    QByteArray stdoutActual;

    // Encrypted data differs.
    QCOMPARE( m_test->run(Args("-e") << "plugins.itemencrypted.encrypt(input())", &stdoutActual, nullptr, input), 0 );
    QVERIFY(!stdoutActual.isEmpty());
    QVERIFY(stdoutActual != input);

    QCOMPARE( m_test->run(Args("-e") << "plugins.itemencrypted.decrypt(plugins.itemencrypted.encrypt(input()))", &stdoutActual, nullptr, input), 0 );
    QCOMPARE(stdoutActual, input);
}

void ItemEncryptedTests::encryptDecryptItems()
{
#ifdef Q_OS_MAC
    SKIP("Ctrl+L shortcut doesn't seem work on OS X");
#endif

    if ( !isGpgInstalled() )
        SKIP("gpg2 is required to run the test");

    RUN("-e" << "plugins.itemencrypted.generateTestKeys()", "\n");

    // Load commands from the plugin generating keys.
    RUN("keys" << "Ctrl+P" << "ENTER", "");
    RUN("commands().length", "4\n");

    const auto tab = testTab(1);
    RUN("setCurrentTab" << tab, "");
    RUN("tab" << tab << "add" << "TEST", "");
    RUN("tab" << tab << "read" << "0", "TEST");

    // Encrypt.
    RUN("keys" << "Ctrl+L", "");
    WAIT_ON_OUTPUT("tab" << tab << "read" << "?" << "0", "application/x-copyq-encrypted\n");

    // Decrypt and check text.
    RUN("keys" << "Ctrl+L", "");
    WAIT_ON_OUTPUT("tab" << tab << "read" << "?" << "0", "text/plain\n");
    RUN("tab" << tab << "read" << "0", "TEST");

    // Encrypt and add note.
    RUN("keys" << "Ctrl+L", "");
    WAIT_ON_OUTPUT("tab" << tab << "read" << "?" << "0", "application/x-copyq-encrypted\n");
    RUN("keys" << "Shift+F2" << ":NOTE" << "F2", "");
    RUN("tab" << tab << "read" << "application/x-copyq-item-notes" << "0", "NOTE");

    // Decrypt and check note and text.
    RUN("keys" << "Ctrl+L", "");
    WAIT_ON_OUTPUT("tab" << tab << "read" << "0", "TEST");
    RUN("tab" << tab << "read" << "application/x-copyq-item-notes" << "0", "NOTE");
}

bool ItemEncryptedTests::isGpgInstalled() const
{
    QByteArray actualStdout;
    const auto exitCode = m_test->run(Args("-e") << "plugins.itemencrypted.isGpgInstalled()", &actualStdout);
    Q_ASSERT(exitCode == 0);
    Q_ASSERT(actualStdout == "true\n" || actualStdout == "false\n");
    return actualStdout == "true\n";
}

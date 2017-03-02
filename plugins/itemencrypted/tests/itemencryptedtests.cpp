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

#include "itemencryptedtests.h"

#include "tests/test_utils.h"

ItemEncryptedTests::ItemEncryptedTests(const TestInterfacePtr &test, QObject *parent)
    : QObject(parent)
    , m_test(test)
{
}

void ItemEncryptedTests::initTestCase()
{
    if ( qgetenv("COPYQ_TESTS_SKIP_ITEMENCRYPT") == "1" )
        SKIP("Unset COPYQ_TESTS_SKIP_ITEMENCRYPT to run the tests");

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

    RUN("-e" << "plugins.itemencrypted.generateTestKeys()", "");

    // Test gpg errors first.
    RUN("-e" << "plugins.itemencrypted.encrypt(input());print('')", "");

    const QByteArray input("\x00\x01\x02\x03\x04", 5);
    QByteArray stdoutActual;
    QByteArray stderrActual;

    // Encrypted data differs.
    QCOMPARE( m_test->run(Args("-e") << "plugins.itemencrypted.encrypt(input())", &stdoutActual, nullptr, input), 0 );
    QVERIFY(!stdoutActual.isEmpty());
    QVERIFY(stdoutActual != input);

    QCOMPARE( m_test->run(Args("-e") << "plugins.itemencrypted.decrypt(plugins.itemencrypted.encrypt(input()))", &stdoutActual, nullptr, input), 0 );
    QCOMPARE(stdoutActual, input);
}

bool ItemEncryptedTests::isGpgInstalled() const
{
    QByteArray actualStdout;
    const auto exitCode = m_test->run(Args("-e") << "plugins.itemencrypted.isGpgInstalled()", &actualStdout);
    Q_ASSERT(exitCode == 0);
    Q_ASSERT(actualStdout == "true\n" || actualStdout == "false\n");
    return actualStdout == "true\n";
}

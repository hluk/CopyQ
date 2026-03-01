// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

void Tests::expireEncryptionPassword()
{
#ifdef WITH_QCA_ENCRYPTION
    const QString tab1 = testTab(1);
    const QString tab2 = testTab(2);
    const Args args1 = Args("tab") << tab1 << "separator" << " ";
    const Args args2 = Args("tab") << tab2 << "separator" << " ";

    RUN("config" << "encrypt_tabs" << "true", "true\n");

    RUN(args1 << "add" << "A1", "");
    RUN(args2 << "add" << "B1", "");

    // Setting an expiration should not cause any expiration yet
    RUN("config" << "expire_encrypted_tab_seconds" << "1", "1\n");
    KEYS(clipboardBrowserId);
    QTest::qWait(1500);
    KEYS(clipboardBrowserId);

    RUN("config" << "expire_encrypted_tab_seconds" << "2", "2\n");
    KEYS(clipboardBrowserId);

    TEST( m_test->stopServer() );
    m_test->setEnv("COPYQ_PASSWORD", "");
    TEST( m_test->startServer() );

    // Start expiration timer from manual password entry.
    RUN_MULTIPLE(
        [&]{ RUN("show" << tab1, ""); },
        [&]{ KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); }
    );

    RUN_MULTIPLE(
        [&]{ KEYS(clipboardBrowserId); },
        [&]{ RUN("selectedTab", tab1 + "\n"); },
        [&]{ RUN(args1 << "read" << "0", "A1"); },
        [&]{ RUN(args2 << "read" << "0", "B1"); }
    );

    RUN("show" << tab2, "");
    RUN("show" << tab1, "");
    RUN("show" << tab2, "");

    KEYS(clipboardBrowserId);
    QTest::qWait(2500);
    KEYS(clipboardBrowserId);

    RUN_MULTIPLE(
        [&]{ KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); },
        [&]{ RUN(args1 << "read" << "0", "A1"); }
    );
    KEYS(clipboardBrowserId);
    RUN("selectedTab", tab2 + "\n");

    RUN("show" << tab1, "");
    RUN_MULTIPLE(
        [&]{ RUN("selectedTab", tab1 + "\n"); },
        [&]{ KEYS(clipboardBrowserId); },
        [&]{ RUN(args1 << "read" << "0", "A1"); }
    );

    RUN("show" << tab2, "");
    RUN(args2 << "read" << "0", "B1");

    // Expire again: active tab should stay unlocked.
    KEYS(clipboardBrowserId);
    QTest::qWait(2500);
    KEYS(clipboardBrowserId);

    RUN("show" << tab2, "");
    RUN(args2 << "read" << "0", "B1");

    // Switching to the other expired tab should prompt again.
    RUN_MULTIPLE(
        [&]{ KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); },
        [&]{ RUN("show" << tab1, ""); }
    );
    RUN(args1 << "read" << "0", "A1");
    KEYS(clipboardBrowserId);

    // Avoid asking password for a new tab (if prompted recently)
    const QString tab3 = testTab(3);
    RUN("show" << tab3, "");

    KEYS(clipboardBrowserId);
    QTest::qWait(2500);
    KEYS(clipboardBrowserId);

    // Read multiple expired tabs items, wait for password prompt once
    RUN_MULTIPLE(
        [&]{ RUN(args1 << "read" << "0", "A1"); },
        [&]{ RUN(args2 << "read" << "0", "B1"); },
        [&]{ QTest::qWait(200); KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); }
    );

    KEYS(clipboardBrowserId);
    QTest::qWait(2500);
    KEYS(clipboardBrowserId);

    // Expired tabs should require password, even if the configuration changed
    RUN("config" << "expire_encrypted_tab_seconds" << "0", "0\n");
    RUN_MULTIPLE(
        [&]{ KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); },
        [&]{ RUN("show" << tab1, ""); }
    );
    KEYS(clipboardBrowserId);
#else
    SKIP("Encryption support not built-in");
#endif
}

void Tests::expireEncryptionPasswordOnConfigChange()
{
    // Expired tabs should require password,
    // even if the expiration was disabled afterwards
#ifdef WITH_QCA_ENCRYPTION
    const QString tab1 = testTab(1);
    const Args args1{"tab", tab1};
    RUN(args1 << "add" << "A1", "");

    RUN("config" << "encrypt_tabs" << "true", "true\n");

    // Setting an expiration should not cause any expiration yet
    RUN("config" << "expire_encrypted_tab_seconds" << "1", "1\n");
    KEYS(clipboardBrowserId);
    QTest::qWait(1500);
    KEYS(clipboardBrowserId);

    RUN("config" << "expire_encrypted_tab_seconds" << "0", "0\n");
    RUN_MULTIPLE(
        [&]{ KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); },
        [&]{ RUN("show" << tab1, ""); }
    );
    KEYS(clipboardBrowserId);
    RUN(args1 << "read" << "0", "A1");
#else
    SKIP("Encryption support not built-in");
#endif
}

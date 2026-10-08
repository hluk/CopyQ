// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

void CoreTests::expireEncryptionPassword()
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
    const int waitToExpireMs = 2500;
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
    QTest::qWait(waitToExpireMs);
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
    QTest::qWait(waitToExpireMs);
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
    QTest::qWait(waitToExpireMs);
    KEYS(clipboardBrowserId);

    // Read multiple expired tabs items, wait for password prompt once
    RUN_MULTIPLE(
        [&]{ RUN(args1 << "read" << "0", "A1"); },
        [&]{ RUN(args2 << "read" << "0", "B1"); },
        [&]{ QTest::qWait(200); KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER"); }
    );

    KEYS(clipboardBrowserId);
    QTest::qWait(waitToExpireMs);
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

void CoreTests::expireEncryptionPasswordOnConfigChange()
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

void CoreTests::pasteItemsAfterEncryptionPasswordExpires()
{
#ifdef WITH_QCA_ENCRYPTION
#ifdef Q_OS_MAC
    SKIP("Native macOS menus do not support the Edit menu mnemonic");
#else
    const QString group = testTab(1);
    const QString tab = group + "/Encrypted";
    const Args args{"tab", tab};
    const QByteArray text("Clipboard text after unlocking");

    RUN("disable", "");
    RUN("config" << "tab_tree" << "true", "true\n");
    RUN(args << "add" << "Existing item", "");
    RUN("show" << tab, "");
    KEYS(clipboardBrowserId);
    RUN("config" << "encrypt_tabs" << "true", "true\n");
    RUN("config" << "expire_encrypted_tab_seconds" << "2", "2\n");
    TEST( m_test->setClipboard(text) );

    // A pure group hides the current tab without changing its stacked index.
    // Keep the Edit menu focused while the hidden tab locks and unloads.
    KEYS("F3" << filterEditId);
    KEYS(QStringLiteral("mouse|CLICK|tab_tree_item|text=%1").arg(group));
    KEYS("mouse|CLICK|Utils::FilterLineEdit" << filterEditId);
    KEYS("ALT+E" << "focus::QMenu");
    QTest::qWait(2500);
    KEYS("focus::QMenu" << "P" << passwordEntryCurrentId);

    // Tests use a 2000 ms clipboard copy timeout. Time spent unlocking the tab
    // must not count towards that timeout, even though it runs an event loop.
    QTest::qWait(2500);
    KEYS(passwordEntryCurrentId << ":TEST123" << "ENTER");

    RUN("show" << tab, "");
    KEYS(filterEditId << "ESC" << clipboardBrowserId);
    RUN(args << "size", "2\n");
    RUN(args << "read" << "0", text);
    RUN(args << "read" << "1", "Existing item");
#endif
#else
    SKIP("Encryption support not built-in");
#endif
}

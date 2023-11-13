// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "common/commandstatus.h"

#include <QByteArray>
#include <QKeySequence>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVariantMap>

#define NO_ERRORS(ERRORS_OR_EMPTY) !m_test->writeOutErrors(ERRORS_OR_EMPTY)

/**
 * Verify that method call (TestInterface::startServer(), TestInterface::runClient() etc.)
 * didn't fail or print error.
 */
#define TEST(ERRORS_OR_EMPTY) \
    QVERIFY2( NO_ERRORS(ERRORS_OR_EMPTY), "Failed with errors above." )

#define RUN(ARGUMENTS, STDOUT_EXPECTED) \
    TEST( m_test->runClient((Args() << ARGUMENTS), toByteArray(STDOUT_EXPECTED)) )

#define RUN_WITH_INPUT(ARGUMENTS, INPUT, STDOUT_EXPECTED) \
    TEST( m_test->runClient((Args() << ARGUMENTS), toByteArray(STDOUT_EXPECTED), toByteArray(INPUT)) )

#define RUN_EXPECT_ERROR(ARGUMENTS, EXIT_CODE) \
    TEST( m_test->runClientWithError((Args() << ARGUMENTS), (EXIT_CODE)) )

#define RUN_EXPECT_ERROR_WITH_STDERR(ARGUMENTS, EXIT_CODE, STDERR_CONTAINS) \
    TEST( m_test->runClientWithError((Args() << ARGUMENTS), (EXIT_CODE), toByteArray(STDERR_CONTAINS)) )

#define WAIT_FOR_CLIPBOARD(DATA) \
    TEST( m_test->verifyClipboard(DATA, "text/plain") )

#define WAIT_FOR_CLIPBOARD2(DATA, MIME) \
    TEST( m_test->verifyClipboard((DATA), (MIME)) )

#define RETURN_ON_ERROR(CALLBACK, ERROR) \
    do { \
        const auto errors = (CALLBACK); \
        if ( !errors.isEmpty() ) \
            return QByteArray(ERROR) + ":\n" + QByteArray(errors); \
    } while(false)

/// Skip rest of the tests
#define SKIP(MESSAGE) QSKIP(MESSAGE, SkipAll)

#define WAIT_ON_OUTPUT(ARGUMENTS, OUTPUT) \
    TEST( m_test->waitOnOutput((Args() << ARGUMENTS), toByteArray(OUTPUT)) )

#define SKIP_ON_ENV(ENV) \
    if ( qgetenv(ENV) == "1" ) \
        SKIP("Unset " ENV " to run the tests")

/// Interval to wait (in ms) before and after setting clipboard.
#ifdef Q_OS_MAC
// macOS seems to require larger delay before/after setting clipboard
const int waitMsSetClipboard = 1000;
#else
const int waitMsSetClipboard = 250;
#endif

/// Interval to wait (in ms) for pasting clipboard.
const int waitMsPasteClipboard = 1000;

/// Interval to wait (in ms) for client process.
const int waitClientRun = 30000;

using Args = QStringList;

inline QByteArray toByteArray(const QString &text)
{
    return text.toUtf8();
}

inline QByteArray toByteArray(const QByteArray &text)
{
    return text;
}

inline QByteArray toByteArray(const char *text)
{
    return text;
}

/// Naming scheme for test tabs in application.
inline QString testTab(int i)
{
    return "Tab_&" + QString::number(i);
}

inline QString keyNameFor(QKeySequence::StandardKey standardKey)
{
    return QKeySequence(standardKey).toString();
}

#endif // TEST_UTILS_H

/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "common/commandstatus.h"

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVariantMap>

/**
 * Verify that method call (TestInterface::startServer(), TestInterface::runClient() etc.)
 * didn't fail or print error.
 */
#define TEST(ERRORS_OR_EMPTY) \
do { \
    QByteArray errors_ = (ERRORS_OR_EMPTY); \
    if (!errors_.isEmpty()) { \
      QFile ferr; \
      ferr.open(stderr, QIODevice::WriteOnly); \
      ferr.write(errors_ + "\n"); \
      ferr.close(); \
      QVERIFY2(false, "Failed with errors above."); \
    } \
} while (false)

#define RUN(ARGUMENTS, STDOUT_EXPECTED) \
    TEST( m_test->runClient((Args() << ARGUMENTS), toByteArray(STDOUT_EXPECTED)) );

#define RUN_WITH_INPUT(ARGUMENTS, STDOUT_EXPECTED, INPUT) \
    TEST( m_test->runClient((Args() << ARGUMENTS), toByteArray(INPUT), toByteArray(STDOUT_EXPECTED)) );

#define RUN_EXPECT_ERROR(ARGUMENTS, EXIT_CODE) \
    TEST( m_test->runClientWithError((Args() << ARGUMENTS), (EXIT_CODE)) );

#define RUN_EXPECT_ERROR_WITH_STDERR(ARGUMENTS, EXIT_CODE, STDERR_CONTAINS) \
    TEST( m_test->runClientWithError((Args() << ARGUMENTS), (EXIT_CODE), toByteArray(STDERR_CONTAINS)) );

#define WAIT_FOR_CLIPBOARD(DATA) \
    QCOMPARE( waitUntilClipboardSet(DATA), QByteArray(DATA) )

#define WAIT_FOR_CLIPBOARD2(DATA, MIME) \
    QCOMPARE( waitUntilClipboardSet(DATA, MIME), QByteArray(DATA) )

/// Skip rest of the tests
#define SKIP(MESSAGE) QSKIP(MESSAGE, SkipAll)

#define WAIT_UNTIL(ARGUMENTS, CONDITION, STDOUT_ACTUAL) \
do { \
    SleepTimer t_(8000); \
    bool finished_ = false; \
    do { \
        TEST( m_test->getClientOutput((Args() << ARGUMENTS), (&STDOUT_ACTUAL)) ); \
    } while (!(finished_ = (CONDITION)) && t_.sleep()); \
    if (!finished_) \
        TEST("Operation timeout!"); \
} while(false)

#define WAIT_ON_OUTPUT(ARGUMENTS, OUTPUT) \
do { \
    QByteArray out_; \
    const QByteArray expected_(OUTPUT); \
    SleepTimer t_(8000); \
    do { \
        TEST( m_test->getClientOutput((Args() << ARGUMENTS), &out_) ); \
    } while (out_ != expected_ && t_.sleep()); \
    QCOMPARE(out_, expected_); \
} while(false)

/// Interval to wait (in ms) until window is shown and focused.
const int waitMsShow = 1000;

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

#endif // TEST_UTILS_H

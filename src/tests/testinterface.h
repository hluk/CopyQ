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

#ifndef TESTINTERFACE_H
#define TESTINTERFACE_H

#include <QSharedPointer>

/// Verify if method call (TestInterface::runClient() etc.) didn't fail or print error.
#define TEST(errorsOrEmpty) \
do { \
    QByteArray errors = errorsOrEmpty; \
    QVERIFY2(errors.isEmpty(), errors); \
} while (false)

/// Skip rest of the tests
#if QT_VERSION < 0x050000
#   define SKIP(message) QSKIP(message, SkipAll)
#else
#   define SKIP(message) QSKIP(message)
#endif

/**
 * Interface for tests.
 */
class TestInterface {
public:
    virtual ~TestInterface() {}

    /// Start or restart GUI server and return true if successful.
    virtual bool startServer() = 0;

    /// Stop GUI server and return true if server is was stopped or is not running.
    virtual bool stopServer() = 0;

    /// Return true if GUI server is not running.
    virtual bool isServerRunning() = 0;

    /// Run client with given @a arguments and read all errors/warnings.
    virtual QByteArray runClient(const QStringList &arguments, const QByteArray &stdoutExpected) = 0;

    /// Set clipboard through monitor process.
    virtual void setClipboard(const QByteArray &bytes, const QString &mime = QString("text/plain")) = 0;

    /// Return errors/warning from server (otherwise empty output).
    virtual QByteArray readServerErrors() = 0;

    /// Init test.
    virtual QByteArray init() = 0;

    /// Clean up tabs and items. Return error string on error.
    virtual QByteArray cleanup() = 0;

    /// Show and focus main window (GUI server must be running).
    virtual QByteArray show() = 0;

    /// Platform specific key to remove (usually Delete, Backspace on OS X).
    virtual QString shortcutToRemove() = 0;
};

typedef QSharedPointer<TestInterface> TestInterfacePtr;

#endif // TESTINTERFACE_H

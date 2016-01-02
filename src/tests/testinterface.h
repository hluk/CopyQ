/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "common/sleeptimer.h"

#include <QSharedPointer>

class QStringList;

/**
 * Interface for tests.
 */
class TestInterface {
public:
    enum ReadStderrFlag {
        // Read errors from stderr (lines with "Error:", "Warning:" and similar).
        ReadErrors = 0,
        // Read all stderr.
        ReadAllStderr = 1,
        // Read errors from stderr but omit single exception in script.
        ReadErrorsWithoutScriptException = 2
    };

    virtual ~TestInterface() {}

    /// Start or restart GUI server and return true if successful.
    virtual QByteArray startServer() = 0;

    /// Stop GUI server and return true if server is was stopped or is not running.
    virtual QByteArray stopServer() = 0;

    /// Return true if GUI server is not running.
    virtual bool isServerRunning() = 0;

    /// Run client with given @a arguments and input and read outputs and return exit code.
    virtual int run(const QStringList &arguments, QByteArray *stdoutData = NULL,
                    QByteArray *stderrData = NULL, const QByteArray &in = QByteArray()) = 0;

    /// Run client with given @a arguments and read all errors/warnings.
    virtual QByteArray runClient(const QStringList &arguments, const QByteArray &stdoutExpected,
                                 const QByteArray &input = QByteArray()) = 0;

    /// Run client with given @a arguments and expect errors/warnings on server and client side.
    virtual QByteArray runClientWithError(const QStringList &arguments, int expectedExitCode,
                                          const QByteArray &stderrContains = QByteArray()) = 0;

    /// Run client with given @a arguments and read output, check stderr and exit code.
    virtual QByteArray getClientOutput(const QStringList &arguments, QByteArray *stdoutActual) = 0;

    /// Set clipboard through monitor process.
    virtual QByteArray setClipboard(const QByteArray &bytes, const QString &mime = QString("text/plain")) = 0;

    /**
     * Return errors/warning from server (otherwise empty output).
     * If @a readAll is set, read all stderr.
     */
    virtual QByteArray readServerErrors(ReadStderrFlag flag = ReadErrors) = 0;

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

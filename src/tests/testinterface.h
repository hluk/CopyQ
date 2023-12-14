// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TESTINTERFACE_H
#define TESTINTERFACE_H

#include "common/clipboardmode.h"

#include <QByteArray>
#include <QStringList>
#include <QVariant>
#include <QtContainerFwd>

#include <memory>

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

    TestInterface() = default;

    virtual ~TestInterface() = default;

    /// Start or restart GUI server and return true if successful.
    virtual QByteArray startServer() = 0;

    /// Stop GUI server and return true if server is was stopped or is not running.
    virtual QByteArray stopServer() = 0;

    /// Returns an error if server doesn't stop after some interval.
    virtual QByteArray waitForServerToStop() = 0;

    /// Return true if GUI server is not running.
    virtual bool isServerRunning() = 0;

    /// Run client with given @a arguments and input and read outputs and return exit code.
    virtual int run(const QStringList &arguments, QByteArray *stdoutData = nullptr,
                    QByteArray *stderrData = nullptr, const QByteArray &in = QByteArray(),
                    const QStringList &environment = QStringList()) = 0;

    /// Run client with given @a arguments and read all errors/warnings.
    virtual QByteArray runClient(const QStringList &arguments, const QByteArray &stdoutExpected,
                                 const QByteArray &input = QByteArray()) = 0;

    /// Run client with given @a arguments and expect errors/warnings on server and client side.
    virtual QByteArray runClientWithError(const QStringList &arguments, int expectedExitCode,
                                          const QByteArray &stderrContains = QByteArray()) = 0;

    /// Run client with given @a arguments and read output, check stderr and exit code.
    virtual QByteArray getClientOutput(const QStringList &arguments, QByteArray *stdoutActual) = 0;

    /// Waits on client output.
    virtual QByteArray waitOnOutput(const QStringList &arguments, const QByteArray &stdoutExpected) = 0;

    /// Set clipboard through monitor process.
    virtual QByteArray setClipboard(const QMap<QString, QVariant> &data, ClipboardMode mode = ClipboardMode::Clipboard) = 0;

    /// Set clipboard through monitor process.
    virtual QByteArray setClipboard(
            const QByteArray &bytes,
            const QString &mime = QLatin1String("text/plain"),
            ClipboardMode mode = ClipboardMode::Clipboard) = 0;

    /// Verify clipboard content.
    virtual QByteArray verifyClipboard(const QByteArray &data, const QString &mime, bool exact = true) = 0;

    /**
     * Return errors/warning from server (otherwise empty output).
     * If @a readAll is set, read all stderr.
     */
    virtual QByteArray readServerErrors(ReadStderrFlag flag = ReadErrors) = 0;

    /// Init test case. Return error string on error.
    virtual QByteArray initTestCase() = 0;

    /// Clean up test case. Return error string on error.
    virtual QByteArray cleanupTestCase() = 0;

    /// Init test. Return error string on error.
    virtual QByteArray init() = 0;

    /// Clean up tabs and items. Return error string on error.
    virtual QByteArray cleanup() = 0;

    /// Platform specific key to remove (usually Delete, Backspace on OS X).
    virtual QString shortcutToRemove() = 0;

    /// Set environment variable for test.
    virtual void setEnv(const QString &name, const QString &value) = 0;

    virtual bool writeOutErrors(const QByteArray &errors) = 0;

    TestInterface(const TestInterface &) = delete;
    TestInterface &operator=(const TestInterface &) = delete;
};

using TestInterfacePtr = std::shared_ptr<TestInterface>;

#endif // TESTINTERFACE_H

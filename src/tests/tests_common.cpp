// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests/tests_common.h"

#include <QRegularExpression>
#include <QTemporaryFile>

TemporaryFile::TemporaryFile()
{
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    QVERIFY(tmp.open());
    m_fileName = tmp.fileName();
    tmp.close();
}

TemporaryFile::~TemporaryFile()
{
    QFile::remove(m_fileName);
}

bool testStderr(const QByteArray &stderrData, TestInterface::ReadStderrFlag flag)
{
    static const QRegularExpression reFailure("(Warning:|ERROR:|ASSERT|ScriptError:).*", QRegularExpression::CaseInsensitiveOption);
    const QLatin1String scriptError("ScriptError:");

    const auto plain = [](const char *str){
        return QRegularExpression(QRegularExpression::escape(QLatin1String(str)));
    };
    const auto regex = [](const char *str){
        return QRegularExpression(QLatin1String(str));
    };
    // Ignore exceptions and errors from clients in application log
    // (these are expected in some tests).
    static const std::initializer_list<QRegularExpression> ignoreList = {
        regex(R"(CopyQ Note \[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] <Client-[^\n]*)"),

        plain("Event handler maximum recursion reached"),

        // X11 (Linux)
        plain("QtWarning: QXcbXSettings::QXcbXSettings(QXcbScreen*) Failed to get selection owner for XSETTINGS_S atom"),
        plain("QtWarning: QXcbConnection: XCB error:"),
        plain("QtWarning: QXcbClipboard: SelectionRequest too old"),
        plain("QtWarning: libpng warning: iCCP: known incorrect sRGB profile"),
        plain("QtWarning: QMime::convertToMime: unhandled mimetype: text/plain"),

        // Wayland (Linux)
        plain("QtWarning: Wayland does not support QWindow::requestActivate()"),
        plain("QtWarning: Unexpected wl_keyboard.enter event"),
        plain("QtWarning: The compositor sent a wl_pointer.enter"),

        // Windows
        plain("QtWarning: QWindowsWindow::setGeometry: Unable to set geometry"),
        plain("QtWarning: QWinEventNotifier: no event dispatcher, application shutting down? Cannot deliver event."),
        plain("QtWarning: setGeometry: Unable to set geometry"),

        plain("ERROR: QtCritical: QWindowsPipeWriter::write failed. (The pipe is being closed.)"),
        plain("ERROR: QtCritical: QWindowsPipeWriter: asynchronous write failed. (The pipe has been ended.)"),

        plain("[kf.notifications] QtWarning: Received a response for an unknown notification."),
        // KStatusNotifierItem
        plain("[kf.windowsystem] QtWarning: Could not find any platform plugin"),

        regex("QtWarning: QTemporaryDir: Unable to remove .* most likely due to the presence of read-only files."),

        // Windows Qt 5.15.2
        plain("[qt.qpa.mime] QtWarning: Retrying to obtain clipboard."),
        plain("[default] QtWarning: QSystemTrayIcon::setVisible: No Icon set"),

        // macOS
        plain("QtWarning: Failed to get QCocoaScreen for NSObject(0x0)"),
        plain("ERROR: Failed to open session mutex: QSystemSemaphore::handle:: ftok failed"),
        regex(R"(QtWarning: Window position.* outside any known screen.*)"),
        regex(R"(QtWarning: Populating font family aliases took .* ms. Replace uses of missing font family "Font Awesome.*" with one that exists to avoid this cost.)"),

        // New in Qt 5.15.0
        regex(R"(QtWarning: Populating font family aliases took .* ms. Replace uses of missing font family "Monospace" with one that exists to avoid this cost.)"),

        // New in Qt 6.5
        regex("QtWarning: Error in contacting registry"),

        // KNotification bug
        plain(R"(QtWarning: QLayout: Attempting to add QLayout "" to QWidget "", which already has a layout)"),

        // Warnings from itemsync plugin, not sure what it causes
        regex(R"(QtWarning: Could not remove our own lock file .* maybe permissions changed meanwhile)"),
    };
    static QHash<QString, bool> ignoreLog;

    const QString output = QString::fromUtf8(stderrData);
    QRegularExpressionMatchIterator it = reFailure.globalMatch(output);
    while ( it.hasNext() ) {
        const auto match = it.next();

        const QString log = match.captured();

        if ( flag == TestInterface::ReadErrorsWithoutScriptException
             && log.contains(scriptError) )
        {
            return false;
        }

        if ( ignoreLog.contains(log) )
            return ignoreLog[log];

        qDebug() << "Failure in logs:" << log;

        const bool ignore = std::any_of(
            std::begin(ignoreList), std::end(ignoreList),
                [&output](const QRegularExpression &reIgnore){
                    return output.contains(reIgnore);
                });

        ignoreLog[log] = ignore;
        if (!ignore)
            return false;
    }

    return true;
}

int count(const QStringList &items, const QString &pattern)
{
    int from = -1;
    int count = 0;
    const QRegularExpression re(pattern);
    while ( (from = items.indexOf(re, from + 1)) != -1 )
        ++count;
    return count;
}

QStringList splitLines(const QByteArray &nativeText)
{
    return QString::fromUtf8(nativeText).split(QRegularExpression("\r\n|\n|\r"));
}

QByteArray generateData()
{
    static int i = 0;
    const QByteArray id = "tests_"
            + QByteArray::number(QDateTime::currentMSecsSinceEpoch() % 1000);
    return id + '_' + QByteArray::number(++i);
}

QString appWindowTitle(const QString &text)
{
#ifdef Q_OS_MAC
    return QStringLiteral("CopyQ - %1\n").arg(text);
#elif defined(Q_OS_WIN)
    return QStringLiteral("%1 - CopyQ-TEST\n").arg(text);
#else
    return QStringLiteral("%1 — CopyQ-TEST\n").arg(text);
#endif
}

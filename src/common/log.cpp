// SPDX-License-Identifier: GPL-3.0-or-later

#include "log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QString>
#include <QtGlobal>
#include <QVariant>

#include <QStandardPaths>

namespace {

const int logFileSize = 512 * 1024;
const int logFileCount = 10;

QString envString(const char *varName)
{
    const QByteArray bytes = qgetenv(varName);
    return QString::fromUtf8( bytes.constData(), bytes.size() );
}

QString logFileName(int i)
{
    if (i <= 0)
        return ::logFileName();
    return ::logFileName() + QLatin1String(".") + QString::number(i);
}

void rotateLogFiles()
{
    for (int i = logFileCount - 1; i > 0; --i) {
        const QString sourceFileName = logFileName(i - 1);
        const QString targetFileName = logFileName(i);
        QFile::remove(targetFileName);
        QFile::rename(sourceFileName, targetFileName);
    }
}

bool writeLogFile(const QByteArray &message)
{
    QFile f( ::logFileName() );
    if ( !f.open(QIODevice::Append) )
        return false;

    if ( f.write(message) <= 0 )
        return false;

    f.close();
    if ( f.size() > logFileSize )
        rotateLogFiles();

    return true;
}

QString getDefaultLogFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QByteArray createLogMessage(const QByteArray &label, const QByteArray &text)
{
    if ( text.contains('\n') ) {
        const QByteArray continuation = '\n' + label + "   ";
        return label + QByteArray(text).replace('\n', continuation) + '\n';
    }

    return label + text + '\n';
}

QByteArray createSimpleLogMessage(const QByteArray &text, const LogLevel level)
{
    const auto label = logLevelLabel(level) + ": ";
    return createLogMessage(label, text);
}

QByteArray createLogMessage(const QByteArray &text, const LogLevel level)
{
    const auto timeStamp =
        QDateTime::currentDateTime().toString(QStringLiteral("[yyyy-MM-dd hh:mm:ss.zzz] ")).toLatin1();
    const auto label = timeStamp + logLevelLabel(level) + " " + logLabel() + ": ";
    return createLogMessage(label, text);
}

QString getLogFileName()
{
    const QString fileName = envString("COPYQ_LOG_FILE");
    if (!fileName.isEmpty())
        return QDir::fromNativeSeparators(fileName);

    const QString path = getDefaultLogFilePath();
    QDir dir(path);
    dir.mkpath(QStringLiteral("."));

    const QString dateTime = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyyMMdd"));
    return QStringLiteral("%3/copyq-%1-%2.log")
        .arg(dateTime).arg(QCoreApplication::applicationPid()).arg(path);
}

void logAlways(const QByteArray &msgText, const LogLevel level)
{
    const auto msg = createLogMessage(msgText, level);
    const bool writtenToLogFile = qApp && writeLogFile(msg);

    // Log to file and if needed to stderr.
    if ( !writtenToLogFile || level <= LogWarning || hasLogLevel(LogDebug) ) {
        QFile ferr;
        ferr.open(stderr, QIODevice::WriteOnly);
        const auto simpleMsg = createSimpleLogMessage(msgText, level);
        ferr.write(simpleMsg);
    }
}

QFileInfoList logFileNames()
{
    const QDir logDir = QFileInfo(::logFileName()).absoluteDir();
    return logDir.entryInfoList({QStringList("copyq*.log*")}, QDir::Files, QDir::Time);
}

bool removeLogFile(const QFileInfo &logFileInfo)
{
    QFile logFile(logFileInfo.absoluteFilePath());
    if (logFile.remove())
        return true;

    log( QStringLiteral("Failed to remove log file \"%1\": %2")
        .arg(logFile.fileName(), logFile.errorString()), LogError );
    return false;
}

} // namespace

const QString &logFileName()
{
    static QCoreApplication *app = QCoreApplication::instance();
    static QString logFileName = getLogFileName();
    // If the application instance got set later, assume that org and
    // application names also changed and update the log file path.
    if (app != QCoreApplication::instance()) {
        app = QCoreApplication::instance();
        logFileName = getLogFileName();
    }
    return logFileName;
}

QByteArray readLogFile(int maxReadSize)
{
    /*
     * Read all log files (from newest to oldest) and sort lines by timestamp.
     */
    std::vector<QByteArray> sortedLogLines;
    int currentSize = 0;

    for (const QFileInfo &logFile : logFileNames()) {
        QFile f(logFile.absoluteFilePath());
        if ( !f.open(QIODevice::ReadOnly) )
            continue;

        const auto seek = f.size() - maxReadSize;
        if (seek > 0) {
            f.seek(seek);
            f.readLine();  // Skip first partial line.
        }

        while (!f.atEnd()) {
            const QByteArray line = f.readLine();
            // Sort only by timestamp to preserve the relative order of records
            // with the same timestamp.
            constexpr auto timestampSize =
                std::char_traits<char>::length("yyyy-MM-dd hh:mm:ss.zzz");
            const auto it = std::upper_bound(
                sortedLogLines.begin(), sortedLogLines.end(), line,
                [](const QByteArray &a, const QByteArray &b) {
                    return a.mid(1, timestampSize) >= b.mid(1, timestampSize);
                });
            if (it == sortedLogLines.end() && currentSize >= maxReadSize)
                continue;

            currentSize += line.size();
            sortedLogLines.insert(it, line);

            while (currentSize > maxReadSize && !sortedLogLines.empty()) {
                currentSize -= sortedLogLines.back().size();
                sortedLogLines.pop_back();
            }
        }
    }

    QByteArray result;
    while( !sortedLogLines.empty() ) {
        result += sortedLogLines.back();
        sortedLogLines.pop_back();
    }

    return result;
}

bool dropLogsToFileCountAndSize(int maxFileCount, int keepMaxSize)
{
    QFileInfoList logFiles = logFileNames();
    bool success = true;

    if (maxFileCount > 0 && maxFileCount < logFiles.size()) {
        const auto begin = logFiles.cbegin() + maxFileCount;
        for (auto it = begin; it != logFiles.cend(); ++it)
            success = removeLogFile(*it) && success;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        logFiles.erase(begin);
#else
        logFiles.erase(logFiles.begin() + maxFileCount);
#endif
    }

    if (keepMaxSize < 0)
        return success;

    int totalSize = 0;
    for (const QFileInfo &logFile : logFiles)
        totalSize += logFile.size();

    if (totalSize <= keepMaxSize)
        return success;

    for (auto it = logFiles.crbegin(); it != logFiles.crend(); ++it) {
        const auto fileSize = it->size();
        if (removeLogFile(*it)) {
            totalSize -= fileSize;
            if (totalSize <= keepMaxSize)
                break;
        } else {
            success = false;
        }
    }

    return success;
}

int getLogLevel()
{
    const QByteArray logLevelString = qgetenv("COPYQ_LOG_LEVEL").toUpper();

    if ( logLevelString.startsWith("TRAC") )
        return LogTrace;
    if ( logLevelString.startsWith("DEBUG") )
        return LogDebug;
    if ( logLevelString.startsWith("NOT") )
        return LogNote;
    if ( logLevelString.startsWith("WARN") )
        return LogWarning;
    if ( logLevelString.startsWith("ERR") )
        return LogError;

#ifdef COPYQ_DEBUG
    return LogDebug;
#else
    return LogNote;
#endif
}

bool hasLogLevel(LogLevel level)
{
    static const int currentLogLevel = getLogLevel();
    return currentLogLevel >= level;
}

QByteArray logLevelLabel(LogLevel level)
{
    switch(level) {
    case LogWarning:
        return QByteArrayLiteral("Warning");
    case LogError:
        return QByteArrayLiteral("ERROR");
    case LogDebug:
        return QByteArrayLiteral("DEBUG");
    case LogTrace:
        return QByteArrayLiteral("TRACE");
    case LogNote:
    case LogAlways:
        return QByteArrayLiteral("Note");
    }

    Q_ASSERT(false);
    return "";
}

void log(const char *text, LogLevel level)
{
    if ( hasLogLevel(level) )
        logAlways(QByteArray(text), level);
}

void log(const QByteArray &text, LogLevel level)
{
    if ( hasLogLevel(level) )
        logAlways(text, level);
}

void log(const QString &text, const LogLevel level)
{
    if ( hasLogLevel(level) )
        logAlways(text.toUtf8(), level);
}

void setLogLabel(const QByteArray &name)
{
    const auto id = QCoreApplication::applicationPid();
    logLabel() = "<" + name + "-" + QByteArray::number(id) + ">";
}

QByteArray &logLabel()
{
    static QByteArray label;
    return label;
}

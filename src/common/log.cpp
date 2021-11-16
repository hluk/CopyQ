/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "log.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QLockFile>
#include <QFile>
#include <QString>
#include <QtGlobal>
#include <QVariant>

#include <QStandardPaths>

namespace {

/// System-wide mutex
class SystemMutex final {
public:
    SystemMutex()
        : m_lockFile(logFileName() + ".lock")
    {
    }

    /// Lock mutex (blocking).
    bool lock()
    {
        ++m_selfLocked;
        return m_selfLocked > 1 || m_lockFile.lock();
    }

    /// Unlock mutex.
    void unlock()
    {
        --m_selfLocked;
        if (m_selfLocked == 0)
            m_lockFile.unlock();
    }

    QString error() const
    {
        if (m_lockFile.error() == QLockFile::NoError)
            return {};

        if (m_lockFile.error() == QLockFile::PermissionError)
            return QStringLiteral("Insufficient permissions to create lock file");

        return QStringLiteral("Unknown error");
    }

private:
    int m_selfLocked = 0;
    QLockFile m_lockFile;
};

const int logFileSize = 512 * 1024;
const int logFileCount = 10;

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

QString envString(const char *varName)
{
    const QByteArray bytes = qgetenv(varName);
    return QString::fromUtf8( bytes.constData(), bytes.size() );
}

QString logFileName(int i)
{
    if (i <= 0)
        return ::logFileName();
    return ::logFileName() + "." + QString::number(i);
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

bool writeLogFileNoLock(const QByteArray &message)
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

/// Lock guard for SystemMutex.
class SystemMutexLocker final {
public:
    /// Locks mutex (it's possible that the mutex won't be locked because of errors).
    explicit SystemMutexLocker(SystemMutex &mutex)
        : m_mutex(mutex)
        , m_locked( m_mutex.lock() )
    {
        if (!m_locked) {
            writeLogFileNoLock(
                "Failed to lock logs: " + m_mutex.error().toUtf8());
        }
    }

    /// Unlocks mutex.
    ~SystemMutexLocker()
    {
        if (m_locked)
            m_mutex.unlock();
    }

    SystemMutexLocker(const SystemMutexLocker &) = delete;
    SystemMutexLocker &operator=(const SystemMutexLocker &) = delete;

private:
    SystemMutex &m_mutex;
    bool m_locked;
};

SystemMutex &getSessionMutex()
{
    static SystemMutex mutex;
    return mutex;
}

QString getDefaultLogFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString readLogFile(const QString &fileName, int maxReadSize)
{
    QFile f(fileName);
    if ( !f.open(QIODevice::ReadOnly) )
        return QString();

    const auto seek = f.size() - maxReadSize;
    if (seek > 0)
        f.seek(seek);
    const QByteArray content = f.readAll();

    return QString::fromUtf8(content);
}

bool writeLogFile(const QByteArray &message)
{
    SystemMutexLocker lock(getSessionMutex());
    return writeLogFileNoLock(message);
}

QByteArray createLogMessage(const QByteArray &label, const QByteArray &text)
{
    return label + QByteArray(text).replace("\n", "\n" + label + "   ") + "\n";
}

QByteArray createSimpleLogMessage(const QByteArray &text, const LogLevel level)
{
    const auto label = logLevelLabel(level) + ": ";
    return createLogMessage(label, text);
}

QByteArray createLogMessage(const QByteArray &text, const LogLevel level)
{
    const auto timeStamp =
            QDateTime::currentDateTime().toString(" [yyyy-MM-dd hh:mm:ss.zzz] ").toUtf8();
    const auto label = "CopyQ " + logLevelLabel(level) + timeStamp + logLabel() + ": ";
    return createLogMessage(label, text);
}

} // namespace

QString logFileName()
{
    const QString fileName = envString("COPYQ_LOG_FILE");
    if (!fileName.isEmpty())
        return QDir::fromNativeSeparators(fileName);

    const QString path = getDefaultLogFilePath();
    QDir dir(path);
    dir.mkpath(".");

    return path + "/copyq.log";
}

QString readLogFile(int maxReadSize)
{
    SystemMutexLocker lock(getSessionMutex());

    QString content;
    for (int i = 0; i < logFileCount; ++i) {
        const int toRead = maxReadSize - content.size();
        content.prepend( readLogFile(logFileName(i), toRead) );
        if ( maxReadSize <= content.size() )
            break;
    }

    return content;
}

bool removeLogFiles()
{
    SystemMutexLocker lock(getSessionMutex());

    for (int i = 0; i < logFileCount; ++i) {
        QFile logFile( logFileName(i) );
        if ( logFile.exists() && !logFile.remove() )
            return false;
    }

    return true;
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
        return "Warning";
    case LogError:
        return "ERROR";
    case LogDebug:
        return "DEBUG";
    case LogTrace:
        return "TRACE";
    case LogNote:
    case LogAlways:
        return "Note";
    }

    Q_ASSERT(false);
    return "";
}

void log(const QString &text, const LogLevel level)
{
    if ( !hasLogLevel(level) )
        return;

    const auto msgText = text.toUtf8();
    const auto msg = createLogMessage(msgText, level);
    const bool writtenToLogFile = writeLogFile(msg);

    // Log to file and if needed to stderr.
    if ( (!writtenToLogFile || level <= LogWarning || hasLogLevel(LogDebug))
            && canUseStandardOutput() )
    {
        QFile ferr;
        ferr.open(stderr, QIODevice::WriteOnly);
        const auto simpleMsg = createSimpleLogMessage(msgText, level);
        ferr.write(simpleMsg);
    }
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

bool canUseStandardOutput()
{
    static const bool useOutput = qEnvironmentVariableIsEmpty("COPYQ_NO_OUTPUT");
    return useOutput;
}

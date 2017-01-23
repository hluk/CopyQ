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

#include "log.h"

#include "platform/platformsystemmutex.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QString>
#include <QtGlobal>

#if QT_VERSION < 0x050000
#   include <QDesktopServices>
#else
#   include <QStandardPaths>
#endif

namespace {

const int logFileSize = 128 * 1024;
const int logFileCount = 4;

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

/// Lock guard for SystemMutex.
class SystemMutexLocker {
public:
    /// Locks mutex (it's possible that the mutex won't be locked because of errors).
    SystemMutexLocker(const PlatformSystemMutexPtr &mutex)
        : m_mutex(mutex)
        , m_locked(!m_mutex.isNull() && m_mutex->lock())
    {
    }

    /// Unlocks mutex.
    ~SystemMutexLocker()
    {
        if (isLocked())
            m_mutex->unlock();
    }

    bool isLocked() const { return m_locked; }

private:
    PlatformSystemMutexPtr m_mutex;
    bool m_locked;
};

QString getDefaultLogFilePath()
{
#if QT_VERSION < 0x050000
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#elif QT_VERSION < 0x050400
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
}

QString readLogFile(const QString &fileName)
{
    QFile f(fileName);
    if ( !f.open(QIODevice::ReadOnly) )
        return QString();

    const QByteArray content = f.readAll();

    return QString::fromUtf8(content);
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

PlatformSystemMutexPtr doGetSessionSystemMutex()
{
    const QString mutexName = QCoreApplication::applicationName() + "_mutex";
    return getPlatformSystemMutex(mutexName);
}

PlatformSystemMutexPtr getSessionSystemMutex()
{
    static PlatformSystemMutexPtr sessionMutex = doGetSessionSystemMutex();
    return sessionMutex;
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

QString readLogFile()
{
    SystemMutexLocker lock( getSessionSystemMutex() );

    QString content;
    for (int i = 0; i < logFileCount; ++i)
        content.prepend( readLogFile(logFileName(i)) );

    content.prepend(logFileName() + "\n\n");

    return content;
}

bool hasLogLevel(LogLevel level)
{
    static const int currentLogLevel = getLogLevel();
    return currentLogLevel >= level;
}

QString logLevelLabel(LogLevel level)
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
    default:
        return "Note";
    }
}

QString createLogMessage(const QString &text, const LogLevel level)
{
    const QString timeStamp =
            QDateTime::currentDateTime().toString(" [yyyy-MM-dd hh:mm:ss.zzz]");

    const QString label = "CopyQ " + logLevelLabel(level) + timeStamp + ": ";

    return label + QString(text).replace("\n", "\n" + label + "   ") + "\n";
}

void log(const QString &text, const LogLevel level)
{
    if ( !hasLogLevel(level) )
        return;

    SystemMutexLocker lock( getSessionSystemMutex() );

    const QByteArray msg = createLogMessage(text, level).toUtf8();

    QFile f( logFileName() );
    const bool writtenToLogFile = f.open(QIODevice::Append) && f.write(msg);
    if (writtenToLogFile)
        f.close();

    // Log to file and if needed to stderr.
    if ( !writtenToLogFile || level <= LogWarning ) {
        QFile ferr;
        ferr.open(stderr, QIODevice::WriteOnly);
        ferr.write(msg);
    }

    if ( writtenToLogFile && f.size() > logFileSize )
        rotateLogFiles();
}

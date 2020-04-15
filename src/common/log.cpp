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
#include <QFile>
#include <QString>
#include <QSystemSemaphore>
#include <QtGlobal>
#include <QVariant>

#include <QStandardPaths>

#include <cstring>
#include <cmath>
#include <memory>

/// System-wide mutex
class SystemMutex final {
public:
    /**
     * Open system mutex if exists, otherwise create one.
     * Name of mutex is same as current session of application.
     */
    SystemMutex(const QString &name, QSystemSemaphore::AccessMode mode)
        : m_semaphore(name, 1, mode)
    {
    }

    /// Lock mutex (blocking).
    bool lock()
    {
        return m_semaphore.acquire();
    }

    /// Unlock mutex.
    bool unlock()
    {
        return m_semaphore.release();
    }

    QString error() const
    {
        return m_semaphore.error() == QSystemSemaphore::NoError
                ? QString()
                : m_semaphore.errorString();
    }

private:
    QSystemSemaphore m_semaphore;
};

class SystemMutex;
using SystemMutexPtr = std::shared_ptr<SystemMutex>;
Q_DECLARE_METATYPE(SystemMutexPtr)

namespace {

const int logFileSize = 512 * 1024;
const int logFileCount = 10;

const char propertySessionMutex[] = "CopyQ_Session_Mutex";

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
class SystemMutexLocker final {
public:
    /// Locks mutex (it's possible that the mutex won't be locked because of errors).
    explicit SystemMutexLocker(const SystemMutexPtr &mutex)
        : m_mutex(mutex)
        , m_locked( m_mutex != nullptr && m_mutex->lock() )
    {
    }

    /// Unlocks mutex.
    ~SystemMutexLocker()
    {
        if (isLocked())
            m_mutex->unlock();
    }

    bool isLocked() const { return m_locked; }

    SystemMutexLocker(const SystemMutexLocker &) = delete;
    SystemMutexLocker &operator=(const SystemMutexLocker &) = delete;

private:
    SystemMutexPtr m_mutex;
    bool m_locked;
};

SystemMutexPtr initSessionMutexHelper(QSystemSemaphore::AccessMode accessMode)
{
    const QString mutexName = QCoreApplication::applicationName() + "_mutex";
    const auto sessionMutex = std::make_shared<SystemMutex>(mutexName, accessMode);

    const QString error = sessionMutex->error();
    const bool create = accessMode == QSystemSemaphore::Create;
    if ( !error.isEmpty() ) {
        const QString action = create ? "create" : "open";
        log("Failed to " + action + " session mutex: " + error, LogError);
    } else {
        COPYQ_LOG_VERBOSE(
                    QString("%1 session mutex: %2")
                    .arg(create ? "Created" : "Opened", mutexName) );
    }

    if (qApp)
        qApp->setProperty( propertySessionMutex, QVariant::fromValue(sessionMutex) );

    return sessionMutex;
}

SystemMutexPtr initSessionMutex(QSystemSemaphore::AccessMode accessMode)
{
    static bool initializing = false;
    if (initializing)
        return nullptr;

    initializing = true;
    const auto sessionMutex = initSessionMutexHelper(accessMode);
    initializing = false;

    return sessionMutex;
}

SystemMutexPtr getSessionMutex()
{
    if (qApp) {
        const auto sessionMutex =
                qApp->property(propertySessionMutex).value<SystemMutexPtr>();

        if (sessionMutex)
            return sessionMutex;
    }

    return initSessionMutex(QSystemSemaphore::Open);
}

QString getDefaultLogFilePath()
{
#if QT_VERSION < 0x050400
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
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

bool writeLogFile(const QByteArray &message)
{
    SystemMutexLocker lock(getSessionMutex());

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

void createSessionMutex()
{
    initSessionMutex(QSystemSemaphore::Create);
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
    if ( !writtenToLogFile || level <= LogWarning || hasLogLevel(LogDebug) ) {
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

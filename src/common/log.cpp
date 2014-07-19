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

#include "log.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSharedPointer>
#include <QString>
#include <QSystemSemaphore>
#include <QtGlobal>

namespace {

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

QString getLogFileName()
{
    const QString fileName = QString::fromUtf8( qgetenv("COPYQ_LOG_FILE") );
    return QDir::fromNativeSeparators(fileName);
}

/// System-wide mutex
class SystemMutex {
public:
    /**
     * Open system mutex if exists, otherwise create one.
     * Name of mutex is same as current session of application.
     */
    SystemMutex(const QString &name)
        : m_semaphore(name, 1)
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

    QString error() const { return m_semaphore.errorString(); }

private:
    QSystemSemaphore m_semaphore;
};

typedef QSharedPointer<SystemMutex> SystemMutexPtr;

/// Lock guard for SystemMutex.
class SystemMutexLocker {
public:
    /// Locks mutex (it's possible that the mutex won't be locked because of errors).
    SystemMutexLocker(const SystemMutexPtr &mutex)
        : m_mutex(mutex)
        , m_locked(!m_mutex.isNull() && m_mutex->lock())
    {
        if (!m_mutex.isNull() && !isLocked())
            qDebug() << "ERROR: Session mutex:" << m_mutex->error();
    }

    /// Unlocks mutex.
    ~SystemMutexLocker()
    {
        if (isLocked())
            m_mutex->unlock();
    }

    bool isLocked() const { return m_locked; }

private:
    SystemMutexPtr m_mutex;
    bool m_locked;
};

void tryInitMutex(SystemMutexPtr *mutex)
{
    const QString session = QString::fromUtf8(qgetenv("COPYQ_SESSION_NAME"));
    if (!session.isEmpty())
        *mutex = SystemMutexPtr(new SystemMutex(session));
}

} // namespace

bool hasLogLevel(LogLevel level)
{
    static const int currentLogLevel = getLogLevel();
    return currentLogLevel >= level;
}

QString createLogMessage(const QString &label, const QString &text, const LogLevel level)
{
    QString levelId;

    if (level == LogNote)
        levelId = QString(" %1");
    else if (level == LogWarning)
        levelId = QObject::tr("warning: %1");
    else if (level == LogError)
        levelId = QObject::tr("ERROR: %1");
    else if (level == LogDebug)
        levelId = QString("DEBUG: %1");
    else if (level == LogTrace)
        levelId = QString("TRACE: %1");

    return label + " " + levelId.arg(text) + "\n";
}

void log(const QString &text, const LogLevel level)
{
    if ( !hasLogLevel(level) )
        return;

    static SystemMutexPtr sessionMutex;
    if (sessionMutex.isNull())
        tryInitMutex(&sessionMutex);
    SystemMutexLocker lock(sessionMutex);

    // Always print time at debug log level.
    const QString label = hasLogLevel(LogDebug)
            ? QDateTime::currentDateTime().toString("CopyQ [yyyy-MM-dd hh:mm:ss.zzz]")
            : QString("CopyQ");

    const QString msg = createLogMessage(label, text, level);

    // Log to file or stderr.
    static const QString logFileName = getLogFileName();
    QFile f;
    if ( logFileName.isEmpty() ) {
        f.open(stderr, QIODevice::WriteOnly);
    } else {
        f.setFileName(logFileName);
        f.open(QIODevice::Append);
    }
    f.write( msg.toUtf8() );
}

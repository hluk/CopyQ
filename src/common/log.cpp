// SPDX-License-Identifier: GPL-3.0-or-later

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

QString &logFileNameVariable() {
    static QString logFileName;
    return logFileName;
}

/// System-wide mutex
class SystemMutex final {
public:
    SystemMutex()
        : m_lockFile(logFileName() + QLatin1String(".lock"))
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

QByteArray readLogFile(const QString &fileName, int maxReadSize)
{
    QFile f(fileName);
    if ( !f.open(QIODevice::ReadOnly) )
        return {};

    const auto seek = f.size() - maxReadSize;
    if (seek > 0)
        f.seek(seek);

    return f.readAll();
}

bool writeLogFile(const QByteArray &message)
{
    SystemMutexLocker lock(getSessionMutex());
    return writeLogFileNoLock(message);
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
        QDateTime::currentDateTime().toString(QStringLiteral(" [yyyy-MM-dd hh:mm:ss.zzz] ")).toLatin1();
    const auto label = "CopyQ " + logLevelLabel(level) + timeStamp + logLabel() + ": ";
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

    return path + QStringLiteral("/copyq.log");
}

void logAlways(const QByteArray &msgText, const LogLevel level)
{
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

} // namespace

void initLogging()
{
    logFileNameVariable() = getLogFileName();
}

const QString &logFileName()
{
    if ( logFileNameVariable().isEmpty() )
        logFileNameVariable() = getLogFileName();
    return logFileNameVariable();
}

QByteArray readLogFile(int maxReadSize)
{
    SystemMutexLocker lock(getSessionMutex());

    QByteArray content;
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

bool canUseStandardOutput()
{
    static const bool useOutput = qEnvironmentVariableIsEmpty("COPYQ_NO_OUTPUT");
    return useOutput;
}

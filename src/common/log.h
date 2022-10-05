// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOG_H
#define LOG_H

class QByteArray;
class QString;

enum LogLevel {
    LogAlways,
    LogError,
    LogWarning,
    LogNote,
    LogDebug,
    LogTrace
};

void initLogging();

const QString &logFileName();

QByteArray readLogFile(int maxReadSize);

bool removeLogFiles();

void createSessionMutex();

bool hasLogLevel(LogLevel level);

QByteArray logLevelLabel(LogLevel level);

#define COPYQ_LOG(msg) do { if ( hasLogLevel(LogDebug) ) log(msg, LogDebug); } while (false)
#define COPYQ_LOG_VERBOSE(msg) do { if ( hasLogLevel(LogTrace) ) log(msg, LogTrace); } while (false)

void log(const char *text, LogLevel level = LogNote);
void log(const QString &text, LogLevel level = LogNote);
void log(const QByteArray &text, LogLevel level = LogNote);

void setLogLabel(const QByteArray &name);

QByteArray &logLabel();

/// True if stdout and stderr can be used.
bool canUseStandardOutput();

#endif // LOG_H

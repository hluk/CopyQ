// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once


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

const QString &logFileName();

QByteArray readLogFile(int maxReadSize);

bool dropLogsToFileCountAndSize(int maxFileCount, int keepMaxSize);

void createSessionMutex();

int getLogLevel();

bool hasLogLevel(LogLevel level);

QByteArray logLevelLabel(LogLevel level);

#define COPYQ_LOG(msg) do { if ( hasLogLevel(LogDebug) ) log(msg, LogDebug); } while (false)
#define COPYQ_LOG_VERBOSE(msg) do { if ( hasLogLevel(LogTrace) ) log(msg, LogTrace); } while (false)

void log(const char *text, LogLevel level = LogNote);
void log(const QString &text, LogLevel level = LogNote);
void log(const QByteArray &text, LogLevel level = LogNote);

void setLogLabel(const QByteArray &name);

QByteArray &logLabel();

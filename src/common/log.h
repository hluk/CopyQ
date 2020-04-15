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

QString logFileName();

QString readLogFile(int maxReadSize);

bool removeLogFiles();

void createSessionMutex();

bool hasLogLevel(LogLevel level);

QByteArray logLevelLabel(LogLevel level);

#define COPYQ_LOG(msg) do { if ( hasLogLevel(LogDebug) ) log(msg, LogDebug); } while (false)
#define COPYQ_LOG_VERBOSE(msg) do { if ( hasLogLevel(LogTrace) ) log(msg, LogTrace); } while (false)

void log(const QString &text, LogLevel level = LogNote);

void setLogLabel(const QByteArray &name);

QByteArray &logLabel();

#endif // LOG_H

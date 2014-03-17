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

#ifndef LOG_H
#define LOG_H

#include <QByteArray>
#include <QtGlobal>

class QString;

enum LogLevel {
#ifdef COPYQ_LOG_DEBUG
    LogDebug,
#endif
    LogNote,
    LogWarning,
    LogError
};

#ifdef COPYQ_LOG_DEBUG
inline bool isLogVerbose()
{
    static const bool verbose = qgetenv("COPYQ_VERBOSE") == "1";
    return verbose;
}
#   define COPYQ_LOG(msg) log(msg, LogDebug)
#   define COPYQ_LOG_VERBOSE(msg) do { if ( isLogVerbose() ) log(msg, LogDebug); } while (false)
#else
#   define COPYQ_LOG(msg)
#   define COPYQ_LOG_VERBOSE(msg)
#endif

QString createLogMessage(const QString &label, const QString &text, const LogLevel level);

void log(const QString &text, const LogLevel level = LogNote);

#endif // LOG_H

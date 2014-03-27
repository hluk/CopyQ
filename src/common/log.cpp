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
#include <QDir>
#include <QFile>
#include <QString>
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

#ifdef COPYQ_LOG_DEBUG
    return LogDebug;
#else
    return LogNote;
#endif
}

QString getLogFileName()
{
    const QString fileName = QString::fromLocal8Bit( qgetenv("COPYQ_LOG_FILE") );
    return QDir::fromNativeSeparators(fileName);
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
    f.write( msg.toLocal8Bit() );
}

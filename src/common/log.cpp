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

#include <QFile>
#include <QString>

QString createLogMessage(const QString &label, const QString &text, const LogLevel level)
{
    QString levelId;

    if (level == LogNote)
        levelId = QString("%1");
    else if (level == LogWarning)
        levelId = QObject::tr("warning: %1");
    else if (level == LogError)
        levelId = QObject::tr("ERROR: %1");
#ifdef COPYQ_LOG_DEBUG
    else if (level == LogDebug)
        levelId = QString("DEBUG: %1");
#endif

    return label + " " + levelId.arg(text) + "\n";
}

void log(const QString &text, const LogLevel level)
{
    const QString msg = createLogMessage("CopyQ", text, level);

    QFile f;
    f.open(stderr, QIODevice::WriteOnly);
    f.write( msg.toLocal8Bit() );
}

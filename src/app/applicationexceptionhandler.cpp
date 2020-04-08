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

#include "applicationexceptionhandler.h"

#include "common/log.h"

#include <QCoreApplication>
#include <QFile>
#include <QMetaObject>

void logException(const char *what)
{
    QFile f;
    f.open(stderr, QIODevice::WriteOnly);
    f.write(what ? what : "Unknown exception");
    f.write("\n");
    f.close();

    log( QString("Exception: ") + what, LogError );
}

void detail::ApplicationExceptionHandlerBase::exit(int exitCode)
{
    QMetaObject::invokeMethod(this, "exitSlot", Q_ARG(int, exitCode));
}

void detail::ApplicationExceptionHandlerBase::exitSlot(int exitCode)
{
    qApp->exit(exitCode);
}

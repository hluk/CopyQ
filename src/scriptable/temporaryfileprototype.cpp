/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "temporaryfileprototype.h"

#include <QScriptEngine>
#include <QScriptValue>
#include <QTemporaryFile>

Q_DECLARE_METATYPE(QTemporaryFile*)

TemporaryFilePrototype::TemporaryFilePrototype(QObject *parent)
    : FilePrototype(parent)
{
}

bool TemporaryFilePrototype::autoRemove() const
{
    return thisTemporaryFile()->autoRemove();
}

QString TemporaryFilePrototype::fileTemplate() const
{
    return thisTemporaryFile()->fileTemplate();
}

void TemporaryFilePrototype::setAutoRemove(bool autoRemove)
{
    thisTemporaryFile()->setAutoRemove(autoRemove);
}

void TemporaryFilePrototype::setFileTemplate(const QString &name)
{
    thisTemporaryFile()->setFileTemplate(name);
}

QTemporaryFile *TemporaryFilePrototype::thisTemporaryFile() const
{
    return qscriptvalue_cast<QTemporaryFile*>(thisObject().data());
}

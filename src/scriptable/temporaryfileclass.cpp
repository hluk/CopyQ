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

#include "temporaryfileclass.h"

#include <QDir>
#include <QTemporaryFile>

COPYQ_DECLARE_SCRIPTABLE_CLASS(TemporaryFileClass)

TemporaryFileClass::TemporaryFileClass(QScriptEngine *engine, const QScriptValue &objectPrototype)
    : ScriptableClass(engine, objectPrototype)
{
}

QScriptValue TemporaryFileClass::newInstance(const QString &templateName)
{
    return ScriptableClass::newInstance( new QTemporaryFile(QDir(m_currentPath).absoluteFilePath(templateName)) );
}

QScriptValue TemporaryFileClass::newInstance()
{
    return ScriptableClass::newInstance( new QTemporaryFile() );
}

const QString &TemporaryFileClass::getCurrentPath() const
{
    return m_currentPath;
}

void TemporaryFileClass::setCurrentPath(const QString &path)
{
    m_currentPath = path;
}

QScriptValue TemporaryFileClass::createInstance(const QScriptContext &context)
{
    return context.argumentCount() > 0
            ? newInstance(context.argument(0).toString())
            : newInstance();
}

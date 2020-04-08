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

#include "dirclass.h"

#include <QDir>

COPYQ_DECLARE_SCRIPTABLE_CLASS(DirClass)

DirClass::DirClass(QScriptEngine *engine, const QScriptValue &objectPrototype)
    : ScriptableClass(engine, objectPrototype)
    , m_currentPath(QDir::currentPath())
{
}

QScriptValue DirClass::newInstance(const QDir &dir)
{
    return ScriptableClass::newInstance( new DirWrapper(dir) );
}

QScriptValue DirClass::newInstance(const QString &path)
{
    return ScriptableClass::newInstance( new DirWrapper(QDir(m_currentPath).absoluteFilePath(path)) );
}

QScriptValue DirClass::newInstance()
{
    return ScriptableClass::newInstance( new DirWrapper(m_currentPath) );
}

const QString &DirClass::getCurrentPath() const
{
    return m_currentPath;
}

void DirClass::setCurrentPath(const QString &path)
{
    m_currentPath = path;
}

QScriptValue DirClass::createInstance(const QScriptContext &context)
{
    return context.argumentCount() > 0
            ? newInstance(context.argument(0).toString())
            : newInstance();
}

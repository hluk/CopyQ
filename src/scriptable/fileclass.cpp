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

#include "fileclass.h"

#include "fileprototype.h"

#include <QtScript>

Q_DECLARE_METATYPE(QFile*)
Q_DECLARE_METATYPE(FileClass*)

FileClass::FileClass(const QString &currentPath, QScriptEngine *engine)
    : QObject(engine)
    , QScriptClass(engine)
    , m_currentPath(currentPath)
{
    qScriptRegisterMetaType<QFile*>(engine, toScriptValue, fromScriptValue);

    proto = engine->newQObject(new FilePrototype(this),
                               QScriptEngine::QtOwnership,
                               QScriptEngine::SkipMethodsInEnumeration
                               | QScriptEngine::ExcludeSuperClassMethods
                               | QScriptEngine::ExcludeSuperClassProperties);
    QScriptValue global = engine->globalObject();
    proto.setPrototype(global.property("Object").property("prototype"));

    ctor = engine->newFunction(construct, proto);
    ctor.setData(engine->toScriptValue(this));
}

QScriptValue FileClass::constructor()
{
    return ctor;
}

QScriptValue FileClass::newInstance(const QString &path)
{
    QFile *file = new QFile( QDir(m_currentPath).absoluteFilePath(path), engine() );
    QScriptValue data = engine()->newQObject(file, QScriptEngine::ScriptOwnership);
    return engine()->newObject(this, data);
}

QScriptValue FileClass::prototype() const
{
    return proto;
}

const QString &FileClass::getCurrentPath() const
{
    return m_currentPath;
}

void FileClass::setCurrentPath(const QString &path)
{
    m_currentPath = path;
}

QScriptValue FileClass::construct(QScriptContext *ctx, QScriptEngine *)
{
    FileClass *cls = qscriptvalue_cast<FileClass*>(ctx->callee().data());
    if (!cls)
        return QScriptValue();
    QScriptValue arg = ctx->argument(0);
    return cls->newInstance(arg.toString());
}

QScriptValue FileClass::toScriptValue(QScriptEngine *eng, QFile* const &file)
{
    return eng->newQObject(file, QScriptEngine::QtOwnership, QScriptEngine::PreferExistingWrapperObject);
}

void FileClass::fromScriptValue(const QScriptValue &value, QFile *&file)
{
    file = qobject_cast<QFile*>(value.toQObject());
}

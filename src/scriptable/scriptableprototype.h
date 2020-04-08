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

#ifndef SCRIPTABLEPROTOTYPE_H
#define SCRIPTABLEPROTOTYPE_H

template <typename ScriptableClass>
ScriptableClass *getByteArrayClass(QScriptEngine *eng)
{
    QScriptValue ctor = eng->globalObject().property("ByteArray");
    return qscriptvalue_cast<ByteArrayClass*>(ctor.data());
}

QScriptValue FilePrototype::newByteArray(const QByteArray &bytes)
{
    ByteArrayClass *cls = getByteArrayClass(engine());
    return cls ? cls->newInstance(bytes) : engine()->newVariant(QVariant::fromValue(bytes));
}

QByteArray FilePrototype::toByteArray(const QScriptValue &value)
{
    ByteArrayClass *cls = getByteArrayClass(engine());
    return (cls && value.scriptClass() == cls)
            ? *qscriptvalue_cast<QByteArray*>(value.data())
            : value.toString().toUtf8();
}

#endif // SCRIPTABLEPROTOTYPE_H

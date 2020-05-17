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

#include "scriptablebytearray.h"

#include "common/textdata.h"

#include <QJSEngine>
#include <QJSValue>

ScriptableByteArray::ScriptableByteArray(const QByteArray &bytes)
    : m_self(bytes)
{
}

ScriptableByteArray::ScriptableByteArray(const QString &text)
    : m_self(text.toUtf8())
{
}

ScriptableByteArray::ScriptableByteArray(int size)
    : m_self(size, /*ch=*/0)
{
}

ScriptableByteArray::ScriptableByteArray(const ScriptableByteArray &other)
    : QObject()
    , m_self(other.m_self)
{
}

void ScriptableByteArray::chop(int n)
{
    m_self.chop(n);
}

bool ScriptableByteArray::equals(const QJSValue &other)
{
    const auto byteArray = engine()->fromScriptValue<ScriptableByteArray*>(other);
    return byteArray && m_self == *byteArray->data();
}

QJSValue ScriptableByteArray::left(int len) const
{
    return newByteArray( m_self.left(len) );
}

QJSValue ScriptableByteArray::mid(int pos, int len) const
{
    return newByteArray( m_self.mid(pos, len) );
}

QJSValue ScriptableByteArray::remove(int pos, int len)
{
    return newByteArray( m_self.remove(pos, len) );
}

QJSValue ScriptableByteArray::right(int len) const
{
    return newByteArray( m_self.right(len) );
}

QJSValue ScriptableByteArray::simplified() const
{
    return newByteArray( m_self.simplified() );
}

QJSValue ScriptableByteArray::toBase64() const
{
    return newByteArray( m_self.toBase64() );
}

QJSValue ScriptableByteArray::toLower() const
{
    return newByteArray( m_self.toLower() );
}

QJSValue ScriptableByteArray::toUpper() const
{
    return newByteArray( m_self.toUpper() );
}

QJSValue ScriptableByteArray::trimmed() const
{
    return newByteArray( m_self.trimmed() );
}

void ScriptableByteArray::truncate(int pos)
{
    m_self.truncate(pos);
}

QJSValue ScriptableByteArray::valueOf() const
{
    return toString();
}

int ScriptableByteArray::size() const
{
    return m_self.size();
}

QString ScriptableByteArray::toString() const
{
    return getTextData(m_self);
}

QString ScriptableByteArray::toLatin1String() const
{
    return QString::fromLatin1(m_self);
}

QJSValue ScriptableByteArray::length() const
{
    return m_self.length();
}

void ScriptableByteArray::setLength(QJSValue size)
{
    m_self.resize(size.toInt());
}

QJSEngine *ScriptableByteArray::engine() const
{
    return qjsEngine(this);
}

QJSValue ScriptableByteArray::newByteArray(const QByteArray &bytes) const
{
    return engine()->newQObject( new ScriptableByteArray(bytes) );
}

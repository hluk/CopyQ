// SPDX-License-Identifier: GPL-3.0-or-later

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
    return static_cast<int>(m_self.length());
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

const QByteArray *getByteArray(const QJSValue &value)
{
    const auto obj1 = value.toQObject();
    const auto obj = qobject_cast<ScriptableByteArray*>(obj1);
    return obj ? obj->data() : nullptr;
}

QByteArray toByteArray(const QJSValue &value)
{
    const auto byteArray = qobject_cast<ScriptableByteArray*>(value.toQObject());
    return byteArray ? *byteArray->data() : value.toString().toUtf8();
}

QString toString(const QJSValue &value)
{
    const QByteArray *bytes = getByteArray(value);
    return (bytes == nullptr) ? value.toString() : getTextData(*bytes);
}

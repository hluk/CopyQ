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

ScriptableByteArray::ScriptableByteArray(const QVariant &value)
    : m_variant(value)
{
}

ScriptableByteArray::ScriptableByteArray(int size)
    : m_self(size, /*ch=*/0)
{
}

ScriptableByteArray::ScriptableByteArray(const ScriptableByteArray &other)
    : QObject()
    , m_self(other.m_self)
    , m_variant(other.m_variant)
{
}

void ScriptableByteArray::chop(int n)
{
    self().chop(n);
}

bool ScriptableByteArray::equals(const QJSValue &other)
{
    const auto byteArray = engine()->fromScriptValue<ScriptableByteArray*>(other);
    return byteArray && self() == *byteArray->data();
}

QJSValue ScriptableByteArray::left(int len)
{
    return newByteArray( self().left(len) );
}

QJSValue ScriptableByteArray::mid(int pos, int len)
{
    return newByteArray( self().mid(pos, len) );
}

QJSValue ScriptableByteArray::remove(int pos, int len)
{
    return newByteArray( self().remove(pos, len) );
}

QJSValue ScriptableByteArray::right(int len)
{
    return newByteArray( self().right(len) );
}

QJSValue ScriptableByteArray::simplified()
{
    return newByteArray( self().simplified() );
}

QJSValue ScriptableByteArray::toBase64()
{
    return newByteArray( self().toBase64() );
}

QJSValue ScriptableByteArray::toLower()
{
    return newByteArray( self().toLower() );
}

QJSValue ScriptableByteArray::toUpper()
{
    return newByteArray( self().toUpper() );
}

QJSValue ScriptableByteArray::trimmed()
{
    return newByteArray( self().trimmed() );
}

void ScriptableByteArray::truncate(int pos)
{
    self().truncate(pos);
}

QJSValue ScriptableByteArray::valueOf()
{
    return toString();
}

int ScriptableByteArray::size()
{
    return self().size();
}

QString ScriptableByteArray::toString()
{
    return getTextData(self());
}

QString ScriptableByteArray::toLatin1String()
{
    return QString::fromLatin1(self());
}

QJSValue ScriptableByteArray::length()
{
    return static_cast<int>(self().length());
}

void ScriptableByteArray::setLength(QJSValue size)
{
    self().resize(size.toInt());
}

QJSEngine *ScriptableByteArray::engine() const
{
    return qjsEngine(this);
}

QJSValue ScriptableByteArray::newByteArray(const QByteArray &bytes) const
{
    return engine()->newQObject( new ScriptableByteArray(bytes) );
}

QByteArray &ScriptableByteArray::self()
{
    // Convert lazily to QByteArray from QVariant when needed.
    // This avoids expensive conversion from DataFile type.
    if (m_variant.isValid()) {
        m_self = m_variant.toByteArray();
        m_variant.clear();
    }

    return m_self;
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
    if (byteArray)
        return *byteArray->data();

    const QVariant variant = value.toVariant();
    if (variant.canConvert<QByteArray>())
        return variant.toByteArray();

    return value.toString().toUtf8();
}

QString toString(const QJSValue &value)
{
    const QByteArray *bytes = getByteArray(value);
    return (bytes == nullptr) ? value.toString() : getTextData(*bytes);
}

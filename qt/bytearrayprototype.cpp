/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

#include "bytearrayprototype.h"

#include "common/common.h"

#include <QtScript/QScriptEngine>

Q_DECLARE_METATYPE(QByteArray*)

ByteArrayPrototype::ByteArrayPrototype(QObject *parent)
    : QObject(parent)
{
}

ByteArrayPrototype::~ByteArrayPrototype()
{
}

//! [0]
QByteArray *ByteArrayPrototype::thisByteArray() const
{
    const auto v = qscriptvalue_cast<QByteArray*>(thisObject().data());
    if (!v && engine() && engine()->currentContext())
        engine()->currentContext()->throwError("Invalid ByteArray object");
    return v;
}
//! [0]

void ByteArrayPrototype::chop(int n)
{
    const auto v = thisByteArray();
    if (v)
        v->chop(n);
}

bool ByteArrayPrototype::equals(const QByteArray &other)
{
    const auto v = thisByteArray();
    return v && *v == other;
}

QByteArray ByteArrayPrototype::left(int len) const
{
    const auto v = thisByteArray();
    return v ? v->left(len) : QByteArray();
}

//! [1]
QByteArray ByteArrayPrototype::mid(int pos, int len) const
{
    const auto v = thisByteArray();
    return v ? v->mid(pos, len) : QByteArray();
}

QScriptValue ByteArrayPrototype::remove(int pos, int len)
{
    const auto v = thisByteArray();
    if (v)
        v->remove(pos, len);
    return thisObject();
}
//! [1]

QByteArray ByteArrayPrototype::right(int len) const
{
    const auto v = thisByteArray();
    return v ? v->right(len) : QByteArray();
}

QByteArray ByteArrayPrototype::simplified() const
{
    const auto v = thisByteArray();
    return v ? v->simplified() : QByteArray();
}

QByteArray ByteArrayPrototype::toBase64() const
{
    const auto v = thisByteArray();
    return v ? v->toBase64() : QByteArray();
}

QByteArray ByteArrayPrototype::toLower() const
{
    const auto v = thisByteArray();
    return v ? v->toLower() : QByteArray();
}

QByteArray ByteArrayPrototype::toUpper() const
{
    const auto v = thisByteArray();
    return v ? v->toUpper() : QByteArray();
}

QByteArray ByteArrayPrototype::trimmed() const
{
    const auto v = thisByteArray();
    return v ? v->trimmed() : QByteArray();
}

void ByteArrayPrototype::truncate(int pos)
{
    const auto v = thisByteArray();
    if (v)
        v->truncate(pos);
}

QString ByteArrayPrototype::toLatin1String() const
{
    const auto v = thisByteArray();
    return v ? QString::fromLatin1(*v) : QString();
}

//! [2]
QScriptValue ByteArrayPrototype::valueOf() const
{
    return thisObject().data();
}

int ByteArrayPrototype::size() const
{
    const auto v = thisByteArray();
    return v ? v->size() : 0;
}

QString ByteArrayPrototype::text() const
{
    const auto v = thisByteArray();
    return v ? dataToText(*v) : QString();
}
//! [2]

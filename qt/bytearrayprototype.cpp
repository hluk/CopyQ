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
    return qscriptvalue_cast<QByteArray*>(thisObject().data());
}
//! [0]

void ByteArrayPrototype::chop(int n)
{
    thisByteArray()->chop(n);
}

bool ByteArrayPrototype::equals(const QByteArray &other)
{
    return *thisByteArray() == other;
}

QByteArray ByteArrayPrototype::left(int len) const
{
    return thisByteArray()->left(len);
}

//! [1]
QByteArray ByteArrayPrototype::mid(int pos, int len) const
{
    return thisByteArray()->mid(pos, len);
}

QScriptValue ByteArrayPrototype::remove(int pos, int len)
{
    thisByteArray()->remove(pos, len);
    return thisObject();
}
//! [1]

QByteArray ByteArrayPrototype::right(int len) const
{
    return thisByteArray()->right(len);
}

QByteArray ByteArrayPrototype::simplified() const
{
    return thisByteArray()->simplified();
}

QByteArray ByteArrayPrototype::toBase64() const
{
    return thisByteArray()->toBase64();
}

QByteArray ByteArrayPrototype::toLower() const
{
    return thisByteArray()->toLower();
}

QByteArray ByteArrayPrototype::toUpper() const
{
    return thisByteArray()->toUpper();
}

QByteArray ByteArrayPrototype::trimmed() const
{
    return thisByteArray()->trimmed();
}

void ByteArrayPrototype::truncate(int pos)
{
    thisByteArray()->truncate(pos);
}

QString ByteArrayPrototype::toLatin1String() const
{
    return QString::fromLatin1(*thisByteArray());
}

//! [2]
QScriptValue ByteArrayPrototype::valueOf() const
{
    return thisObject().data();
}
//! [2]

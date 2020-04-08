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

#include "fileprototype.h"

#include "../qt/bytearrayclass.h"

#include <QFile>
#include <QScriptEngine>
#include <QScriptValue>

Q_DECLARE_METATYPE(ByteArrayClass*)
Q_DECLARE_METATYPE(QByteArray*)
Q_DECLARE_METATYPE(QFile*)

namespace {

ByteArrayClass *getByteArrayClass(QScriptEngine *eng)
{
    QScriptValue ctor = eng->globalObject().property("ByteArray");
    return qscriptvalue_cast<ByteArrayClass*>(ctor.data());
}

} // namespace

FilePrototype::FilePrototype(QObject *parent)
    : QObject(parent)
{
}

bool FilePrototype::open()
{
    const auto v = thisFile();
    return v && v->open(QIODevice::ReadWrite);
}

bool FilePrototype::openReadOnly()
{
    const auto v = thisFile();
    return v && v->open(QIODevice::ReadOnly);
}

bool FilePrototype::openWriteOnly()
{
    const auto v = thisFile();
    return v && v->open(QIODevice::WriteOnly);
}

bool FilePrototype::openAppend()
{
    const auto v = thisFile();
    return v && v->open(QIODevice::Append);
}

void FilePrototype::close()
{
    const auto v = thisFile();
    if (v)
        v->close();
}

QScriptValue FilePrototype::read(qint64 maxSize)
{
    const auto v = thisFile();
    return v ? newByteArray(v->read(maxSize)) : QScriptValue();
}

QScriptValue FilePrototype::readLine(qint64 maxSize)
{
    const auto v = thisFile();
    return v ? newByteArray(v->readLine(maxSize)) : QScriptValue();
}

QScriptValue FilePrototype::readAll()
{
    const auto v = thisFile();
    return v ? newByteArray(v->readAll()) : QScriptValue();
}

qint64 FilePrototype::write(const QScriptValue &value)
{
    const auto v = thisFile();
    return v ? v->write(toByteArray(value)) : 0;
}

bool FilePrototype::atEnd() const
{
    const auto v = thisFile();
    return v && v->atEnd();
}

qint64 FilePrototype::bytesAvailable() const
{
    const auto v = thisFile();
    return v ? v->bytesAvailable() : 0;
}

qint64 FilePrototype::bytesToWrite() const
{
    const auto v = thisFile();
    return v ? v->bytesToWrite() : 0;
}

bool FilePrototype::canReadLine() const
{
    const auto v = thisFile();
    return v && v->canReadLine();
}

QScriptValue FilePrototype::errorString() const
{
    const auto v = thisFile();
    return v ? v->errorString() : QScriptValue();
}

bool FilePrototype::isOpen() const
{
    const auto v = thisFile();
    return v && v->isOpen();
}

bool FilePrototype::isReadable() const
{
    const auto v = thisFile();
    return v && v->isReadable();
}

bool FilePrototype::isWritable() const
{
    const auto v = thisFile();
    return v && v->isWritable();
}

QScriptValue FilePrototype::peek(qint64 maxSize)
{
    const auto v = thisFile();
    return v ? newByteArray(v->peek(maxSize)) : QScriptValue();
}

qint64 FilePrototype::pos() const
{
    const auto v = thisFile();
    return v ? v->pos() : 0;
}

bool FilePrototype::reset()
{
    const auto v = thisFile();
    return v && v->reset();
}

bool FilePrototype::seek(qint64 pos)
{
    const auto v = thisFile();
    return v && v->seek(pos);
}

void FilePrototype::setTextModeEnabled(bool enabled)
{
    const auto v = thisFile();
    if (v)
        v->setTextModeEnabled(enabled);
}

qint64 FilePrototype::size() const
{
    const auto v = thisFile();
    return v ? v->size() : 0;
}

QScriptValue FilePrototype::fileName() const
{
    const auto v = thisFile();
    return v ? v->fileName() : QScriptValue();
}

bool FilePrototype::exists() const
{
    const auto v = thisFile();
    return v && v->exists();
}

bool FilePrototype::flush()
{
    const auto v = thisFile();
    return v && v->flush();
}

bool FilePrototype::remove()
{
    const auto v = thisFile();
    return v && v->remove();
}

QFile *FilePrototype::thisFile() const
{
    const auto v = qscriptvalue_cast<QFile*>(thisObject().data());
    if (!v && engine() && engine()->currentContext())
        engine()->currentContext()->throwError("Invalid File object");
    return v;
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

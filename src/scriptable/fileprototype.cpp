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
    return thisFile()->open(QIODevice::ReadWrite);
}

bool FilePrototype::openReadOnly()
{
    return thisFile()->open(QIODevice::ReadOnly);
}

bool FilePrototype::openWriteOnly()
{
    return thisFile()->open(QIODevice::WriteOnly);
}

bool FilePrototype::openAppend()
{
    return thisFile()->open(QIODevice::Append);
}

void FilePrototype::close()
{
    return thisFile()->close();
}

QScriptValue FilePrototype::read(qint64 maxSize)
{
    return newByteArray(thisFile()->read(maxSize));
}

QScriptValue FilePrototype::readLine(qint64 maxSize)
{
    return newByteArray(thisFile()->readLine(maxSize));
}

QScriptValue FilePrototype::readAll()
{
    return newByteArray(thisFile()->readAll());
}

qint64 FilePrototype::write(const QScriptValue &value)
{
    return thisFile()->write(toByteArray(value));
}

bool FilePrototype::atEnd() const
{
    return thisFile()->atEnd();
}

qint64 FilePrototype::bytesAvailable() const
{
    return thisFile()->bytesAvailable();
}

qint64 FilePrototype::bytesToWrite() const
{
    return thisFile()->bytesToWrite();
}

bool FilePrototype::canReadLine() const
{
    return thisFile()->canReadLine();
}

QScriptValue FilePrototype::errorString() const
{
    return thisFile()->errorString();
}

bool FilePrototype::isOpen() const
{
    return thisFile()->isOpen();
}

bool FilePrototype::isReadable() const
{
    return thisFile()->isReadable();
}

bool FilePrototype::isWritable() const
{
    return thisFile()->isWritable();
}

QScriptValue FilePrototype::peek(qint64 maxSize)
{
    return newByteArray(thisFile()->peek(maxSize));
}

qint64 FilePrototype::pos() const
{
    return thisFile()->pos();
}

bool FilePrototype::reset()
{
    return thisFile()->reset();
}

bool FilePrototype::seek(qint64 pos)
{
    return thisFile()->seek(pos);
}

void FilePrototype::setTextModeEnabled(bool enabled)
{
    return thisFile()->setTextModeEnabled(enabled);
}

qint64 FilePrototype::size() const
{
    return thisFile()->size();
}

QScriptValue FilePrototype::fileName() const
{
    return thisFile()->fileName();
}

bool FilePrototype::exists() const
{
    return thisFile()->exists();
}

bool FilePrototype::flush()
{
    return thisFile()->flush();
}

bool FilePrototype::remove()
{
    return thisFile()->remove();
}

QFile *FilePrototype::thisFile() const
{
    return qscriptvalue_cast<QFile*>(thisObject().data());
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

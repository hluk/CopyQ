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

#include "scriptablefile.h"

#include "scriptable/scriptablebytearray.h"

#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QJSValue>

Q_DECLARE_METATYPE(QByteArray*)
Q_DECLARE_METATYPE(QFile*)

ScriptableFile::ScriptableFile(const QString &path)
    : m_path(path)
{
}

bool ScriptableFile::open()
{
    return self()->open(QIODevice::ReadWrite);
}

bool ScriptableFile::openReadOnly()
{
    return self()->open(QIODevice::ReadOnly);
}

bool ScriptableFile::openWriteOnly()
{
    return self()->open(QIODevice::WriteOnly);
}

bool ScriptableFile::openAppend()
{
    return self()->open(QIODevice::Append);
}

void ScriptableFile::close()
{
    self()->close();
}

QJSValue ScriptableFile::read(qint64 maxSize)
{
    return newByteArray(self()->read(maxSize));
}

QJSValue ScriptableFile::readLine(qint64 maxSize)
{
    return newByteArray(self()->readLine(maxSize));
}

QJSValue ScriptableFile::readAll()
{
    return newByteArray(self()->readAll());
}

qint64 ScriptableFile::write(const QJSValue &value)
{
    return self()->write(toByteArray(value));
}

bool ScriptableFile::atEnd()
{
    return self()->atEnd();
}

qint64 ScriptableFile::bytesAvailable()
{
    return self()->bytesAvailable();
}

qint64 ScriptableFile::bytesToWrite()
{
    return self()->bytesToWrite();
}

bool ScriptableFile::canReadLine()
{
    return self()->canReadLine();
}

QJSValue ScriptableFile::errorString()
{
    return self()->errorString();
}

bool ScriptableFile::isOpen()
{
    return self()->isOpen();
}

bool ScriptableFile::isReadable()
{
    return self()->isReadable();
}

bool ScriptableFile::isWritable()
{
    return self()->isWritable();
}

QJSValue ScriptableFile::peek(qint64 maxSize)
{
    return newByteArray(self()->peek(maxSize));
}

qint64 ScriptableFile::pos()
{
    return self()->pos();
}

bool ScriptableFile::reset()
{
    return self()->reset();
}

bool ScriptableFile::seek(qint64 pos)
{
    return self()->seek(pos);
}

void ScriptableFile::setTextModeEnabled(bool enabled)
{
    self()->setTextModeEnabled(enabled);
}

qint64 ScriptableFile::size()
{
    return self()->size();
}

QJSValue ScriptableFile::fileName()
{
    return self()->fileName();
}

bool ScriptableFile::exists()
{
    return self()->exists();
}

bool ScriptableFile::flush()
{
    return self()->flush();
}

bool ScriptableFile::remove()
{
    return self()->remove();
}

QFile *ScriptableFile::self()
{
    if (m_self)
        return m_self;

    setFile( new QFile(this) );
    return m_self;
}

void ScriptableFile::setFile(QFile *file)
{
    Q_ASSERT(m_self == nullptr);
    m_self = file;

    if ( !m_path.isNull() ) {
        m_self->setFileName(m_path);
    }
}

QJSEngine *ScriptableFile::engine() const
{
    return qjsEngine(this);
}

QJSValue ScriptableFile::newByteArray(const QByteArray &bytes)
{
    return engine()->newQObject( new ScriptableByteArray(bytes) );
}

QByteArray ScriptableFile::toByteArray(const QJSValue &value)
{
    const auto byteArray = qobject_cast<ScriptableByteArray*>(value.toQObject());
    return byteArray ? *byteArray->data() : value.toString().toUtf8();
}

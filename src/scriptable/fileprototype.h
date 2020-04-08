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

#ifndef FILEPROTOTYPE_H
#define FILEPROTOTYPE_H

#include <QObject>
#include <QScriptable>
#include <QScriptValue>

class QFile;

class FilePrototype : public QObject, public QScriptable
{
    Q_OBJECT
public:
    explicit FilePrototype(QObject *parent = nullptr);

public slots:
    bool open();
    bool openReadOnly();
    bool openWriteOnly();
    bool openAppend();

    void close();

    QScriptValue read(qint64 maxSize);
    QScriptValue readLine(qint64 maxSize = 0);
    QScriptValue readAll();

    qint64 write(const QScriptValue &value);

    bool atEnd() const;
    qint64 bytesAvailable() const;
    qint64 bytesToWrite() const;
    bool canReadLine() const;
    QScriptValue errorString() const;
    bool isOpen() const;
    bool isReadable() const;
    bool isWritable() const;
    QScriptValue peek(qint64 maxSize);
    qint64 pos() const;
    bool reset();
    bool seek(qint64 pos);
    void setTextModeEnabled(bool enabled);
    qint64 size() const;

    QScriptValue fileName() const;
    bool exists() const;
    bool flush();
    bool remove();

private:
    QFile *thisFile() const;

    QScriptValue newByteArray(const QByteArray &bytes);
    QByteArray toByteArray(const QScriptValue &value);
};

#endif // FILEPROTOTYPE_H

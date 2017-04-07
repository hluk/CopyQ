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

#ifndef SCRIPTABLEFILE_H
#define SCRIPTABLEFILE_H

#include <QJSValue>
#include <QObject>

class QJSEngine;
class QFile;

class ScriptableFile : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit ScriptableFile(const QString &path = QString());

public slots:
    bool open();
    bool openReadOnly();
    bool openWriteOnly();
    bool openAppend();

    void close();

    QJSValue read(qint64 maxSize);
    QJSValue readLine(qint64 maxSize = 0);
    QJSValue readAll();

    qint64 write(const QJSValue &value);

    bool atEnd();
    qint64 bytesAvailable();
    qint64 bytesToWrite();
    bool canReadLine();
    QJSValue errorString();
    bool isOpen();
    bool isReadable();
    bool isWritable();
    QJSValue peek(qint64 maxSize);
    qint64 pos();
    bool reset();
    bool seek(qint64 pos);
    void setTextModeEnabled(bool enabled);
    qint64 size();

    QJSValue fileName();
    bool exists();
    bool flush();
    bool remove();

    virtual QFile *self();

protected:
    void setFile(QFile *file);
    QJSEngine *engine() const;

private:
    QJSValue newByteArray(const QByteArray &bytes);
    QByteArray toByteArray(const QJSValue &value);

    QFile *m_self = nullptr;
    QString m_path;
};

#endif // SCRIPTABLEFILE_H

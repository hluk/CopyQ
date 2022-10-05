// SPDX-License-Identifier: GPL-3.0-or-later

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

    QFile *m_self = nullptr;
    QString m_path;
};

#endif // SCRIPTABLEFILE_H

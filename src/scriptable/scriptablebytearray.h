// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCRIPTABLEBYTEARRAY_H
#define SCRIPTABLEBYTEARRAY_H

#include <QByteArray>
#include <QJSValue>
#include <QObject>

class ScriptableByteArray final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QJSValue length READ length WRITE setLength)

public:
    Q_INVOKABLE ScriptableByteArray() {}
    Q_INVOKABLE explicit ScriptableByteArray(const QByteArray &bytes);
    Q_INVOKABLE explicit ScriptableByteArray(const QString &text);
    Q_INVOKABLE explicit ScriptableByteArray(int size);

    Q_INVOKABLE ScriptableByteArray(const ScriptableByteArray &other);

    const QByteArray *data() const { return &m_self; }

public slots:
    void chop(int n);
    bool equals(const QJSValue &other);
    QJSValue left(int len) const;
    QJSValue mid(int pos, int len = -1) const;
    QJSValue remove(int pos, int len);
    QJSValue right(int len) const;
    QJSValue simplified() const;
    QJSValue toBase64() const;
    QJSValue toLower() const;
    QJSValue toUpper() const;
    QJSValue trimmed() const;
    void truncate(int pos);
    QJSValue valueOf() const;

    int size() const;

    QString toString() const;
    QString toLatin1String() const;

    QJSValue length() const;
    void setLength(QJSValue size);

private:
    QJSEngine *engine() const;

    QJSValue newByteArray(const QByteArray &bytes) const;

    QByteArray m_self;
};

const QByteArray *getByteArray(const QJSValue &value);

QByteArray toByteArray(const QJSValue &value);

QString toString(const QJSValue &value);

#endif // SCRIPTABLEBYTEARRAY_H

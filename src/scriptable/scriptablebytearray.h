// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCRIPTABLEBYTEARRAY_H
#define SCRIPTABLEBYTEARRAY_H

#include <QByteArray>
#include <QJSValue>
#include <QObject>
#include <QVariant>

class ScriptableByteArray final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QJSValue length READ length WRITE setLength)

public:
    Q_INVOKABLE ScriptableByteArray() {}
    Q_INVOKABLE explicit ScriptableByteArray(const QByteArray &bytes);
    Q_INVOKABLE explicit ScriptableByteArray(const QString &text);
    Q_INVOKABLE explicit ScriptableByteArray(const QVariant &value);
    Q_INVOKABLE explicit ScriptableByteArray(int size);

    Q_INVOKABLE ScriptableByteArray(const ScriptableByteArray &other);

    const QByteArray *data() { return &self(); }

public slots:
    void chop(int n);
    bool equals(const QJSValue &other);
    QJSValue left(int len);
    QJSValue mid(int pos, int len = -1);
    QJSValue remove(int pos, int len);
    QJSValue right(int len);
    QJSValue simplified();
    QJSValue toBase64();
    QJSValue toLower();
    QJSValue toUpper();
    QJSValue trimmed();
    void truncate(int pos);
    QJSValue valueOf();

    int size();

    QString toString();
    QString toLatin1String();

    QJSValue length();
    void setLength(QJSValue size);

private:
    QJSEngine *engine() const;

    QJSValue newByteArray(const QByteArray &bytes) const;

    QByteArray &self();

    QByteArray m_self;
    QVariant m_variant;
};

const QByteArray *getByteArray(const QJSValue &value);

QByteArray toByteArray(const QJSValue &value);

QString toString(const QJSValue &value);

#endif // SCRIPTABLEBYTEARRAY_H

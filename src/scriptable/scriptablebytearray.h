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

#endif // SCRIPTABLEBYTEARRAY_H

/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <QVector>
#include <QByteArray>
#include <QVariant>

class Arguments
{
public:
    Arguments();
    Arguments(int &argc, char **argv);

    QByteArray message() const;

    void append(const QByteArray &argument);

    const QByteArray &at(int i) const;

    void back();

    int toInt();
    QString toString();
    QByteArray toByteArray();
    QVariant toVariant();

    void setDefault(const QVariant &default_value);

    int length() const { return m_args.size(); }
    bool atEnd() const { return m_current == length(); }
    bool finished() const { return !error() && atEnd(); }

    void setError() { m_error = true; }
    bool error() const { return m_error; }
    void reset();

private:
    QVector<QByteArray> m_args;
    int m_current;
    bool m_error;
    QVariant m_default_value;
};

QDataStream &operator <<(QDataStream &stream, const Arguments &args);
QDataStream &operator >>(QDataStream &stream, Arguments &args);

// get argument (no arguments left OR argument cannot convert to type T -> error)
// defaul value can be set when the rvalue is const
// example:
//   args >> cmd >> 0 >> row >> "text/plain" >> mime
//   i.e. read cmd, row (default value is 0) and mime (defaul value is "text/plain")
Arguments &operator >>(Arguments &args, int &dest);
Arguments &operator >>(Arguments &args, const int &dest);
Arguments &operator >>(Arguments &args, QString &dest);
Arguments &operator >>(Arguments &args, const QString &dest);
Arguments &operator >>(Arguments &args, QByteArray &dest);
Arguments &operator >>(Arguments &args, const QByteArray &dest);

#endif // MESSAGE_H

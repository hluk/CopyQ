/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#ifndef FILECLASS_H
#define FILECLASS_H

#include <QObject>
#include <QScriptClass>
#include <QScriptValue>

class QFile;
class QScriptContext;
class QScriptEngine;

class FileClass : public QObject, public QScriptClass
{
    Q_OBJECT
public:
    FileClass(const QString &currentPath, QScriptEngine *engine);

    QScriptValue constructor();

    QScriptValue newInstance(const QString &path);

    QScriptValue prototype() const;

    const QString &getCurrentPath() const;
    void setCurrentPath(const QString &path);

private:
    static QScriptValue construct(QScriptContext *ctx, QScriptEngine *eng);

    static QScriptValue toScriptValue(QScriptEngine *eng, QFile* const &file);
    static void fromScriptValue(const QScriptValue &value, QFile* &file);

    QScriptValue proto;
    QScriptValue ctor;

    QString m_currentPath;
};

#endif // FILECLASS_H

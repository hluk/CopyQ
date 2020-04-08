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

#ifndef DIRCLASS_H
#define DIRCLASS_H

#include "scriptableclass.h"

#include "dirprototype.h"

#include <QScriptValue>

class QScriptContext;
class QScriptEngine;

class DirClass final : public ScriptableClass<DirWrapper, DirPrototype>
{
    Q_OBJECT
public:
    DirClass(QScriptEngine *engine, const QScriptValue &objectPrototype);

    QScriptValue newInstance(const QDir &dir);
    QScriptValue newInstance(const QString &path);
    QScriptValue newInstance();

    const QString &getCurrentPath() const;
    void setCurrentPath(const QString &path);

    QString name() const override { return "Dir"; }

private:
    QScriptValue createInstance(const QScriptContext &context) override;

    QString m_currentPath;
};

#endif // DIRCLASS_H

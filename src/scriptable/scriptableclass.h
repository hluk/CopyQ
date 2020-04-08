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

#ifndef SCRIPTABLECLASS_H
#define SCRIPTABLECLASS_H

#include <QObject>
#include <QScriptClass>
#include <QScriptEngine>
#include <QScriptValue>

class QScriptContext;

#define COPYQ_DECLARE_SCRIPTABLE_CLASS(Derived) \
    Q_DECLARE_METATYPE(Derived::ScriptableValueType*)

class ScriptableClassBase : public QObject, public QScriptClass
{
    Q_OBJECT
public:
    explicit ScriptableClassBase(QScriptEngine *engine);

    virtual QScriptValue createInstance(const QScriptContext &context) = 0;

    QScriptValue constructor();

    QScriptValue prototype() const override;

    QScriptValue newInstance(QObject *instance);

protected:
    void init(QObject *prototype, const QScriptValue &objectPrototype);

    static QScriptValue toScriptValue(QScriptEngine *eng, QObject* const &instance);

private:
    QScriptValue proto;
    QScriptValue ctor;
};

template <typename Object, typename ObjectPrototype>
class ScriptableClass : public ScriptableClassBase
{
public:
    using ScriptableValueType = Object;

    ScriptableClass(QScriptEngine *engine, const QScriptValue &objectPrototype)
        : ScriptableClassBase(engine)
    {
        qScriptRegisterMetaType<Object*>(engine, toScriptValue, fromScriptValue);
        init(new ObjectPrototype(this), objectPrototype);
    }

private:
    static QScriptValue toScriptValue(QScriptEngine *eng, Object* const &instance)
    {
        return ScriptableClassBase::toScriptValue(eng, instance);
    }

    static void fromScriptValue(const QScriptValue &value, Object* &instance)
    {
        instance = qobject_cast<Object*>(value.toQObject());
    }
};

#endif // SCRIPTABLECLASS_H

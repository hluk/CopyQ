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

#include "scriptableclass.h"

#include <QScriptContext>

Q_DECLARE_METATYPE(ScriptableClassBase*)

namespace {

QScriptValue construct(QScriptContext *context, QScriptEngine *)
{
    ScriptableClassBase *cls = qscriptvalue_cast<ScriptableClassBase*>(context->callee().data());
    return cls ? cls->createInstance(*context) : QScriptValue();
}

} // namespace

ScriptableClassBase::ScriptableClassBase(QScriptEngine *engine)
    : QObject(engine)
    , QScriptClass(engine)
{
}

QScriptValue ScriptableClassBase::constructor()
{
    return ctor;
}

QScriptValue ScriptableClassBase::prototype() const
{
    return proto;
}

QScriptValue ScriptableClassBase::newInstance(QObject *instance)
{
    instance->setParent(engine());
    QScriptValue data = engine()->newQObject(instance, QScriptEngine::ScriptOwnership);
    return engine()->newObject(this, data);
}

void ScriptableClassBase::init(QObject *prototype, const QScriptValue &objectPrototype)
{
    proto = engine()->newQObject(
                prototype,
                QScriptEngine::QtOwnership,
                QScriptEngine::SkipMethodsInEnumeration);
    proto.setPrototype(objectPrototype);

    ctor = engine()->newFunction(construct, proto);
    ctor.setData(engine()->toScriptValue(this));
}

QScriptValue ScriptableClassBase::toScriptValue(QScriptEngine *eng, QObject * const &instance)
{
    return eng->newQObject(instance, QScriptEngine::QtOwnership, QScriptEngine::PreferExistingWrapperObject);
}

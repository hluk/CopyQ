// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptableitemselection.h"
#include "scriptableproxy.h"

#include "scriptable/scriptablebytearray.h"
#include "scriptable/scriptvaluefactory.h"

#include <QJSEngine>

ScriptableItemSelection::ScriptableItemSelection(const QString &tabName)
    : m_tabName(tabName)
{
}

ScriptableItemSelection::~ScriptableItemSelection()
{
    if (m_proxy)
        m_proxy->destroySelection(m_id);
}

QJSValue ScriptableItemSelection::length()
{
    return m_proxy->selectionGetSize(m_id);
}

QJSValue ScriptableItemSelection::tab()
{
    return m_proxy->selectionGetTabName(m_id);
}

QJSValue ScriptableItemSelection::valueOf()
{
    return toString();
}

QJSValue ScriptableItemSelection::str()
{
    return toString();
}

QString ScriptableItemSelection::toString()
{
    const auto rows = m_proxy->selectionGetRows(m_id);
    const auto tabName = m_proxy->selectionGetTabName(m_id);

    QString rowString;
    auto it1 = rows.constBegin();
    while (it1 != rows.constEnd()) {
        auto it2 = std::adjacent_find(it1, rows.constEnd(), [](int lhs, int rhs) {
            return lhs + 1 != rhs;
        });
        if (it2 == rows.constEnd())
            --it2;
        if (it1 == it2)
            rowString.append(QStringLiteral("%1,").arg(*it1));
        else if (it1 + 1 == it2)
            rowString.append(QStringLiteral("%1,%2,").arg(*it1).arg(*it2));
        else
            rowString.append(QStringLiteral("%1..%2,").arg(*it1).arg(*it2));
        it1 = it2 + 1;
    }
    rowString.chop(1);

    return QStringLiteral("ItemSelection(tab=\"%1\", rows=[%2])")
            .arg(tabName, rowString);
}

QJSValue ScriptableItemSelection::selectAll()
{
    m_proxy->selectionSelectAll(m_id);
    return m_self;
}

QJSValue ScriptableItemSelection::select(const QJSValue &re, const QString &mimeFormat)
{
    const QVariant regexp = re.isRegExp()
        ? fromScriptValue<QRegularExpression>(re, qjsEngine(this))
        : QVariant();
    m_proxy->selectionSelect(m_id, regexp, mimeFormat);
    return m_self;
}

QJSValue ScriptableItemSelection::selectRemovable()
{
    m_proxy->selectionSelectRemovable(m_id);
    return m_self;
}

QJSValue ScriptableItemSelection::invert()
{
    m_proxy->selectionInvert(m_id);
    return m_self;
}

QJSValue ScriptableItemSelection::deselectIndexes(const QJSValue &indexes)
{
    const auto indexes2 = fromScriptValue<QVector<int>>(indexes, qjsEngine(this));
    m_proxy->selectionDeselectIndexes(m_id, indexes2);
    return m_self;
}

QJSValue ScriptableItemSelection::deselectSelection(const QJSValue &selection)
{
    const auto other = qobject_cast<ScriptableItemSelection*>(selection.toQObject());
    m_proxy->selectionDeselectSelection(m_id, other->m_id);
    return m_self;
}

QJSValue ScriptableItemSelection::current()
{
    m_proxy->selectionGetCurrent(m_id);
    return m_self;
}

QJSValue ScriptableItemSelection::removeAll()
{
    m_proxy->selectionRemoveAll(m_id);
    return m_self;
}

QJSValue ScriptableItemSelection::copy()
{
    const int newId = m_proxy->selectionCopy(m_id);
    auto cloneQObj = new ScriptableItemSelection(m_tabName);
    const QJSValue clone = qjsEngine(this)->newQObject(cloneQObj);
    cloneQObj->m_proxy = m_proxy;
    cloneQObj->m_id = newId;
    cloneQObj->m_self = clone;
    return clone;
}

QJSValue ScriptableItemSelection::rows()
{
    const auto rows = m_proxy->selectionGetRows(m_id);
    QJSValue array = qjsEngine(this)->newArray(rows.size());
    for ( int i = 0; i < rows.size(); ++i )
        array.setProperty( static_cast<quint32>(i), QJSValue(rows[i]) );
    return array;
}

QJSValue ScriptableItemSelection::itemAtIndex(int index)
{
    const auto item = m_proxy->selectionGetItemIndex(m_id, index);
    return toScriptValue(item, qjsEngine(this));
}

QJSValue ScriptableItemSelection::setItemAtIndex(int index, const QJSValue &item)
{
    const QVariantMap data = fromScriptValue<QVariantMap>(item, qjsEngine(this));
    m_proxy->selectionSetItemIndex(m_id, index, data);
    return m_self;
}

QJSValue ScriptableItemSelection::items()
{
    const QVariantList dataList = m_proxy->selectionGetItemsData(m_id);
    return toScriptValue(dataList, qjsEngine(this));
}

QJSValue ScriptableItemSelection::setItems(const QJSValue &items)
{
    const QVariantList dataList = fromScriptValue<QVariantList>(items, qjsEngine(this));
    m_proxy->selectionSetItemsData(m_id, dataList);
    return m_self;
}

QJSValue ScriptableItemSelection::itemsFormat(const QJSValue &format)
{
    const QVariantList dataList = m_proxy->selectionGetItemsFormat(m_id, ::toString(format));
    return toScriptValue(dataList, qjsEngine(this));
}

QJSValue ScriptableItemSelection::setItemsFormat(const QJSValue &format, const QJSValue &value)
{
    const QVariant variant = value.isUndefined() ? QVariant() : QVariant(toByteArray(value));
    m_proxy->selectionSetItemsFormat(m_id, ::toString(format), variant);
    return m_self;
}

QJSValue ScriptableItemSelection::move(int row)
{
    m_proxy->selectionMove(m_id, row);
    return m_self;
}

QJSValue ScriptableItemSelection::sort(QJSValue compareFn)
{
    const int size = m_proxy->selectionGetSize(m_id);
    QVector<int> indexes;
    indexes.reserve(size);
    for (int i = 0; i < size; ++i)
        indexes.append(i);

    std::sort( indexes.begin(), indexes.end(), [&](int lhs, int rhs) {
        return compareFn.call({lhs, rhs}).toBool();
    } );
    m_proxy->selectionSort(m_id, indexes);
    return m_self;
}

void ScriptableItemSelection::init(const QJSValue &self, ScriptableProxy *proxy, const QString &currentTabName)
{
    m_self = self;
    m_proxy = proxy;
    connect(m_proxy, &QObject::destroyed, this, [this](){ m_proxy = nullptr; });

    if ( m_tabName.isEmpty() )
        m_tabName = currentTabName;

    m_id = m_proxy->createSelection(m_tabName);
}

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJSValue>
#include <QObject>

class ScriptableProxy;

class ScriptableItemSelection final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJSValue length READ length)
    Q_PROPERTY(QJSValue tab READ tab)

public:
    Q_INVOKABLE explicit ScriptableItemSelection(const QString &tabName = QString());

    ~ScriptableItemSelection();

    void init(const QJSValue &self, ScriptableProxy *proxy, const QString &currentTabName);

public slots:
    QJSValue length();
    QJSValue tab();

    QJSValue valueOf();
    QJSValue str();
    QString toString();

    QJSValue selectAll();
    QJSValue select(const QJSValue &re, const QString &mimeFormat = QString());
    QJSValue selectRemovable();
    QJSValue invert();
    QJSValue deselectIndexes(const QJSValue &indexes);
    QJSValue deselectSelection(const QJSValue &selection);

    QJSValue current();

    QJSValue removeAll();

    QJSValue copy();

    QJSValue rows();

    QJSValue itemAtIndex(int index);

    QJSValue setItemAtIndex(int index, const QJSValue &item);

    QJSValue items();
    QJSValue setItems(const QJSValue &items);

    QJSValue itemsFormat(const QJSValue &format);
    QJSValue setItemsFormat(const QJSValue &format, const QJSValue &value);

    QJSValue move(int row);

    QJSValue sort(QJSValue compareFn);

private:
    int m_id = -1;
    QString m_tabName;
    ScriptableProxy *m_proxy = nullptr;
    QJSValue m_self;
};

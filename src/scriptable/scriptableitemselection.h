/*
    Copyright (c) 2021, Lukas Holecek <hluk@email.cz>

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

private:
    int m_id = -1;
    QString m_tabName;
    ScriptableProxy *m_proxy = nullptr;
    QJSValue m_self;
};

/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#ifndef ITEMFACTORY_H
#define ITEMFACTORY_H

#include <QMap>
#include <QObject>
#include <QSharedPointer>
#include <QVector>

class ItemWidget;
class ItemLoaderInterface;
class QModelIndex;
class QWidget;

typedef QSharedPointer<ItemLoaderInterface> ItemLoaderInterfacePtr;

class ItemFactory : public QObject
{
    Q_OBJECT

public:
    /** Return singleton instance. */
    static ItemFactory *instance();

    static bool hasInstance() { return m_Instance != NULL; }

    ItemFactory();

    ~ItemFactory();

    ItemWidget *createItem(const ItemLoaderInterfacePtr &loader,
                           const QModelIndex &index, QWidget *parent);

    ItemWidget *createItem(const QModelIndex &index, QWidget *parent);

    ItemWidget *nextItemLoader(const QModelIndex &index, ItemWidget *current);

    ItemWidget *previousItemLoader(const QModelIndex &index, ItemWidget *current);

    QStringList formatsToSave() const;

    const QVector<ItemLoaderInterfacePtr> &loaders() const { return m_loaders; }

    void setPluginPriority(const QStringList &pluginNames);

private slots:
    void loaderChildDestroyed(QObject *obj);

private:
    ItemWidget *otherItemLoader(const QModelIndex &index, ItemWidget *current, int dir);
    bool loadPlugins();

    static ItemFactory *m_Instance;
    QVector<ItemLoaderInterfacePtr> m_loaders;
    QMap<QObject *, ItemLoaderInterfacePtr> m_loaderChildren;
};

#endif // ITEMFACTORY_H

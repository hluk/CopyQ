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

#include <QObject>
#include <QVector>
#include <QMap>

class ItemWidget;
class ItemLoaderInterface;
class QModelIndex;
class QWidget;

class ItemFactory : public QObject
{
    Q_OBJECT

public:
    /** Return singleton instance. */
    static ItemFactory *instance();

    static bool hasInstance() { return m_Instance != NULL; }

    ItemFactory();

    ItemWidget *createItem(ItemLoaderInterface *loader,
                           const QModelIndex &index, QWidget *parent);

    ItemWidget *createItem(const QModelIndex &index, QWidget *parent);

    ItemWidget *nextItemLoader(const QModelIndex &index, ItemWidget *current);

    ItemWidget *previousItemLoader(const QModelIndex &index, ItemWidget *current);

    QStringList formatsToSave() const;

    const QVector<ItemLoaderInterface *> &loaders() const { return m_loaders; }

    void setPluginPriority(const QStringList &pluginNames);

private slots:
    void loaderChildDestroyed(QObject *obj);

private:
    ItemWidget *otherItemLoader(const QModelIndex &index, ItemWidget *current, int dir);

    static ItemFactory *m_Instance;
    QVector<ItemLoaderInterface *> m_loaders;
    QMap<QObject *, ItemLoaderInterface *> m_loaderChildren;
};

#endif // ITEMFACTORY_H

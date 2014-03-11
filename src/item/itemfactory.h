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

#ifndef ITEMFACTORY_H
#define ITEMFACTORY_H

#include "item/itemwidget.h"

#include <QMap>
#include <QObject>
#include <QSet>
#include <QVector>

class ItemLoaderInterface;
class ItemWidget;
class QAbstractItemModel;
class QFile;
class QModelIndex;
class QWidget;

/**
 * Loads item plugins (loaders) and instantiates ItemWidget objects using appropriate
 * ItemLoaderInterface::create().
 */
class ItemFactory : public QObject
{
    Q_OBJECT

public:
    /**
     * Loads item plugins.
     */
    explicit ItemFactory(QObject *parent = NULL);

    ~ItemFactory();

    /**
     * Instantiate ItemWidget using given @a loader if possible.
     */
    ItemWidget *createItem(const ItemLoaderInterfacePtr &loader,
                           const QModelIndex &index, QWidget *parent);

    /**
     * Instantiate ItemWidget using appropriate loader or creates simple ItemWidget (DummyItem).
     */
    ItemWidget *createItem(const QModelIndex &index, QWidget *parent);

    /**
     * Uses next item loader to instantiate ItemWidget.
     */
    ItemWidget *nextItemLoader(const QModelIndex &index, ItemWidget *current);

    /**
     * Uses previous item loader to instantiate ItemWidget.
     */
    ItemWidget *previousItemLoader(const QModelIndex &index, ItemWidget *current);

    /**
     * Formats to save in history, union of enabled ItemLoaderInterface objects.
     */
    QStringList formatsToSave() const;

    /**
     * Return list of loaders.
     */
    const QVector<ItemLoaderInterfacePtr> &loaders() const { return m_loaders; }

    /**
     * Sort loaders by priority.
     *
     * Method createItem() tries to instantiate ItemWidget with loader in this order.
     *
     * If priority of a loader is not set here, it is sorted after using
     * ItemLoaderInterface::priotity() after all loader explicitly sorted.
     */
    void setPluginPriority(const QStringList &pluginNames);

    /**
     * Enable or disable instantiation of ItemWidget objects using @a loader.
     */
    void setLoaderEnabled(const ItemLoaderInterfacePtr &loader, bool enabled);

    /**
     * Return true if @a loader is enabled.
     */
    bool isLoaderEnabled(const ItemLoaderInterfacePtr &loader) const;

    /**
     * Return true if no plugins were loaded.
     */
    bool hasLoaders() const { return !m_loaders.isEmpty(); }

    /**
     * Load items using a plugin.
     * @return true only if any plugin (ItemLoaderInterface::loadItems()) returned true
     */
    ItemLoaderInterfacePtr loadItems(QAbstractItemModel *model, QFile *file);

    /**
     * Initialize tab.
     * @return true only if any plugin (ItemLoaderInterface::initializeTab()) returned true
     */
    ItemLoaderInterfacePtr initializeTab(QAbstractItemModel *model);

    /**
     * Return true only if any plugin (ItemLoaderInterface::matches()) returns true;
     */
    bool matches(const QModelIndex &index, const QRegExp &re) const;

private slots:
    /** Called if child ItemWidget destroyed. **/
    void loaderChildDestroyed(QObject *obj);

private:
    ItemWidget *otherItemLoader(const QModelIndex &index, ItemWidget *current, int dir);
    bool loadPlugins();

    /** Return enabled plugins with dummy item loader. */
    QList<ItemLoaderInterfacePtr> enabledLoaders() const;

    /** Calls ItemLoaderInterface::transform() for all plugins in reverse order. */
    ItemWidget *transformItem(ItemWidget *item, const QModelIndex &index);

    QVector<ItemLoaderInterfacePtr> m_loaders;
    ItemLoaderInterfacePtr m_dummyLoader;
    QSet<ItemLoaderInterfacePtr> m_disabledLoaders;
    QMap<QObject *, ItemLoaderInterfacePtr> m_loaderChildren;
};

#endif // ITEMFACTORY_H

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

#ifndef ITEMFACTORY_H
#define ITEMFACTORY_H

#include "item/itemwidget.h"

#include <QMap>
#include <QObject>
#include <QSet>
#include <QVector>

#include <memory>

class ItemLoaderInterface;
class ItemWidget;
class ScriptableProxy;
class QAbstractItemModel;
class QIODevice;
class QModelIndex;
class QSettings;
class QWidget;
struct Command;
struct CommandMenu;

using ItemLoaderList = QVector<ItemLoaderPtr>;

/**
 * Loads item plugins (loaders) and instantiates ItemWidget objects using appropriate
 * ItemLoaderInterface::create().
 */
class ItemFactory final : public QObject
{
    Q_OBJECT

public:
    /**
     * Loads item plugins.
     */
    explicit ItemFactory(QObject *parent = nullptr);

    ~ItemFactory();

    /**
     * Instantiate ItemWidget using given @a loader if possible.
     */
    ItemWidget *createItem(
            const ItemLoaderPtr &loader, const QVariantMap &data, QWidget *parent,
            bool antialiasing, bool transform = true, bool preview = false);

    /**
     * Instantiate ItemWidget using appropriate loader or creates simple ItemWidget (DummyItem).
     */
    ItemWidget *createItem(const QVariantMap &data, QWidget *parent, bool antialiasing,
            bool transform = true, bool preview = false);

    ItemWidget *createSimpleItem(const QVariantMap &data, QWidget *parent, bool antialiasing);

    /**
     * Formats to save in history, union of enabled ItemLoaderInterface objects.
     */
    QStringList formatsToSave() const;

    /**
     * Return list of loaders.
     */
    const ItemLoaderList &loaders() const { return m_loaders; }

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
    void setLoaderEnabled(const ItemLoaderPtr &loader, bool enabled);

    /**
     * Return true if @a loader is enabled.
     */
    bool isLoaderEnabled(const ItemLoaderPtr &loader) const;

    /**
     * Return true if no plugins were loaded.
     */
    bool hasLoaders() const { return !m_loaders.isEmpty(); }

    /**
     * Load items using a plugin.
     * @return the first plugin (or nullptr) for which ItemLoaderInterface::loadItems() returned true
     */
    ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel *model, QIODevice *file, int maxItems);

    /**
     * Initialize tab.
     * @return the first plugin (or nullptr) for which ItemLoaderInterface::initializeTab() returned true
     */
    ItemSaverPtr initializeTab(const QString &tabName, QAbstractItemModel *model, int maxItems);

    /**
     * Return true only if any plugin (ItemLoaderInterface::matches()) returns true;
     */
    bool matches(const QModelIndex &index, const QRegularExpression &re) const;

    QList<ItemScriptable*> scriptableObjects() const;

    /**
     * Adds commands from scripts for command dialog.
     */
    QVector<Command> commands(bool enabled = true) const;

    void emitError(const QString &errorString);

    bool loadPlugins();

    void loadItemFactorySettings(QSettings *settings);

    QObject *createExternalEditor(const QModelIndex &index, const QVariantMap &data, QWidget *parent) const;

    QVariantMap data(const QModelIndex &index) const;

    bool setData(const QVariantMap &data, const QModelIndex &index, QAbstractItemModel *model) const;

signals:
    void error(const QString &errorString);
    void addCommands(const QVector<Command> &commands);

private:
    /** Called if child ItemWidget destroyed. **/
    void loaderChildDestroyed(QObject *obj);

    /** Return enabled plugins with dummy item loader. */
    ItemLoaderList enabledLoaders(bool enabled = true) const;

    /** Calls ItemLoaderInterface::transform() for all plugins in reverse order. */
    ItemWidget *transformItem(ItemWidget *item, const QVariantMap &data);

    void addLoader(const ItemLoaderPtr &loader);

    ItemLoaderList m_loaders;
    ItemLoaderPtr m_dummyLoader;
    ItemLoaderList m_disabledLoaders;
    QMap<QObject *, ItemLoaderPtr> m_loaderChildren;
};

#endif // ITEMFACTORY_H

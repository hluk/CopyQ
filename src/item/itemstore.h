// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtContainerFwd>
#include <memory>

class ItemFactory;
class ItemSaverInterface;
class QAbstractItemModel;
class QString;
using ItemSaverPtr = std::shared_ptr<ItemSaverInterface>;

/** Load items from configuration file. */
ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel &model //!< Model for items.
        , ItemFactory *itemFactory, int maxItems);

/** Save items to configuration file. */
bool saveItems(const QString &tabName, const QAbstractItemModel &model //!< Model containing items to save.
        , const ItemSaverPtr &saver);

/** Remove configuration file for items. */
void removeItems(const QString &tabName //!< See ClipboardBrowser::getID().
        );

/** Move configuration file for items. */
bool moveItems(
        const QString &oldId, //!< See ClipboardBrowser::getID().
        const QString &newId //!< See ClipboardBrowser::getID().
        );

void cleanDataFiles(const QStringList &tabNames);

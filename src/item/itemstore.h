// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef ITEMSTORE_H
#define ITEMSTORE_H

#include "item/itemwidget.h"

class QAbstractItemModel;
class ItemFactory;
class QString;

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

#endif // ITEMSTORE_H

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

#endif // ITEMSTORE_H

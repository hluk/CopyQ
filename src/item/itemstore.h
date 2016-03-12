/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

class ClipboardModel;
class ItemFactory;
class ItemLoaderInterface;
class QString;

/** Load items from configuration file. */
ItemLoaderInterface *loadItems(ClipboardModel &model //!< Model for items.
        , ItemFactory *itemFactory);

/** Save items to configuration file. */
bool saveItems(const ClipboardModel &model //!< Model containing items to save.
        , ItemLoaderInterface *loader);

/** Save items with other plugin with higher priority than current one (@a loader). */
bool saveItemsWithOther(ClipboardModel &model //!< Model containing items to save.
        , ItemLoaderInterface *loader, ItemFactory *itemFactory);

/** Remove configuration file for items. */
void removeItems(const QString &tabName //!< See ClipboardBrowser::getID().
        );

/** Move configuration file for items. */
void moveItems(
        const QString &oldId, //!< See ClipboardBrowser::getID().
        const QString &newId //!< See ClipboardBrowser::getID().
        );

#endif // ITEMSTORE_H

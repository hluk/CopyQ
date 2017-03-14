/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#ifndef CLIPBOARDBROWSERSHARED_H
#define CLIPBOARDBROWSERSHARED_H

#include "gui/theme.h"

#include <QString>

#include <memory>

class ItemFactory;

struct ClipboardBrowserShared {
    explicit ClipboardBrowserShared(ItemFactory *itemFactory);

    void loadFromConfiguration();

    QString editor;
    int maxItems;
    bool textWrap;
    bool viMode;
    bool saveOnReturnKey;
    bool moveItemOnReturnKey;
    bool showSimpleItems;
    int minutesToExpire;

    ItemFactory *itemFactory;

    Theme theme;
};
using ClipboardBrowserSharedPtr = std::shared_ptr<ClipboardBrowserShared>;

#endif // CLIPBOARDBROWSERSHARED_H

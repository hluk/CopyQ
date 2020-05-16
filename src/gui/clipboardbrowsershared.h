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
#ifndef CLIPBOARDBROWSERSHARED_H
#define CLIPBOARDBROWSERSHARED_H

#include "gui/theme.h"

#include <QString>

#include <memory>

class ActionHandler;
class ItemFactory;
class NotificationDaemon;

struct ClipboardBrowserShared {
    QString editor;
    int maxItems = 100;
    bool textWrap = true;
    bool viMode = false;
    bool saveOnReturnKey = false;
    bool moveItemOnReturnKey = false;
    bool showSimpleItems = false;
    bool numberSearch = false;
    int minutesToExpire = 0;
    int saveDelayMsOnItemAdded = 0;
    int saveDelayMsOnItemModified = 0;
    int saveDelayMsOnItemRemoved = 0;
    int saveDelayMsOnItemMoved = 0;
    int saveDelayMsOnItemEdited = 0;
    ItemFactory *itemFactory = nullptr;
    ActionHandler *actions = nullptr;
    NotificationDaemon *notifications = nullptr;
    Theme theme;
};

using ClipboardBrowserSharedPtr = std::shared_ptr<ClipboardBrowserShared>;

#endif // CLIPBOARDBROWSERSHARED_H

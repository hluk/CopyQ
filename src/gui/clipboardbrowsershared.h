// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CLIPBOARDBROWSERSHARED_H
#define CLIPBOARDBROWSERSHARED_H

#include "gui/menuitems.h"
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
    bool rowIndexFromOne = true;
    ItemFactory *itemFactory = nullptr;
    ActionHandler *actions = nullptr;
    NotificationDaemon *notifications = nullptr;
    Theme theme;
    MenuItems menuItems;
};

using ClipboardBrowserSharedPtr = std::shared_ptr<ClipboardBrowserShared>;

#endif // CLIPBOARDBROWSERSHARED_H

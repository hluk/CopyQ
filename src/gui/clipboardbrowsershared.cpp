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

#include "clipboardbrowsershared.h"

#include "common/appconfig.h"

ClipboardBrowserShared::ClipboardBrowserShared(ItemFactory *itemFactory)
    : editor()
    , maxItems(100)
    , textWrap(true)
    , viMode(false)
    , saveOnReturnKey(false)
    , moveItemOnReturnKey(false)
    , showSimpleItems(false)
    , minutesToExpire(0)
    , itemFactory(itemFactory)
{
}

void ClipboardBrowserShared::loadFromConfiguration()
{
    AppConfig appConfig;
    editor = appConfig.option<Config::editor>();
    maxItems = appConfig.option<Config::maxitems>();
    textWrap = appConfig.option<Config::text_wrap>();
    viMode = appConfig.option<Config::vi>();
    saveOnReturnKey = !appConfig.option<Config::edit_ctrl_return>();
    moveItemOnReturnKey = appConfig.option<Config::move>();
    showSimpleItems = appConfig.option<Config::show_simple_items>();
    minutesToExpire = appConfig.option<Config::expire_tab>();
}

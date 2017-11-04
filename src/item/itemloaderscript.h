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
#ifndef ITEMLOADERSCRIPT_H
#define ITEMLOADERSCRIPT_H

#include "itemwidget.h"

#include <QObject>

#include <memory>

class ScriptableProxy;

using ItemLoaderPtr = std::shared_ptr<ItemLoaderInterface>;

/**
 * Returns new loader or nullptr if script couldn't be loaded properly.
 */
ItemLoaderPtr createItemLoaderScript(const QString &scriptFilePath, ScriptableProxy *proxy);

#endif // ITEMLOADERSCRIPT_H

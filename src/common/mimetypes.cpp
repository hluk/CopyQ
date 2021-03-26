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

#include "mimetypes.h"

#include <QLatin1String>

const QLatin1String mimeText("text/plain");
const QLatin1String mimeTextUtf8("text/plain;charset=utf-8");
const QLatin1String mimeHtml("text/html");
const QLatin1String mimeUriList("text/uri-list");
const QLatin1String mimeWindowTitle(COPYQ_MIME_PREFIX "owner-window-title");
const QLatin1String mimeItems(COPYQ_MIME_PREFIX "item");
const QLatin1String mimeItemNotes(COPYQ_MIME_PREFIX "item-notes");
const QLatin1String mimeIcon(COPYQ_MIME_PREFIX "item-icon");
const QLatin1String mimeOwner(COPYQ_MIME_PREFIX "owner");
const QLatin1String mimeClipboardMode(COPYQ_MIME_PREFIX "clipboard-mode");
const QLatin1String mimeCurrentTab(COPYQ_MIME_PREFIX "current-tab");
const QLatin1String mimeSelectedItems(COPYQ_MIME_PREFIX "selected-items");
const QLatin1String mimeCurrentItem(COPYQ_MIME_PREFIX "current-item");
const QLatin1String mimeHidden(COPYQ_MIME_PREFIX "hidden");
const QLatin1String mimeShortcut(COPYQ_MIME_PREFIX "shortcut");
const QLatin1String mimeColor(COPYQ_MIME_PREFIX "color");
const QLatin1String mimeOutputTab(COPYQ_MIME_PREFIX "output-tab");

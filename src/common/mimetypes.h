/*
    Copyright (c) 2019, Lukas Holecek <hluk@email.cz>

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

#ifndef MIMETYPES_H
#define MIMETYPES_H

#define COPYQ_MIME_PREFIX "application/x-copyq-"
constexpr auto mimeText = "text/plain";
constexpr auto mimeHtml = "text/html";
constexpr auto mimeUriList = "text/uri-list";
constexpr auto mimeWindowTitle = COPYQ_MIME_PREFIX "owner-window-title";
constexpr auto mimeItems = COPYQ_MIME_PREFIX "item";
constexpr auto mimeItemNotes = COPYQ_MIME_PREFIX "item-notes";
constexpr auto mimeOwner = COPYQ_MIME_PREFIX "owner";
constexpr auto mimeClipboardMode = COPYQ_MIME_PREFIX "clipboard-mode";
constexpr auto mimeCurrentTab = COPYQ_MIME_PREFIX "current-tab";
constexpr auto mimeSelectedItems = COPYQ_MIME_PREFIX "selected-items";
constexpr auto mimeCurrentItem = COPYQ_MIME_PREFIX "current-item";
constexpr auto mimeHidden = COPYQ_MIME_PREFIX "hidden";
constexpr auto mimeShortcut = COPYQ_MIME_PREFIX "shortcut";
constexpr auto mimeColor = COPYQ_MIME_PREFIX "color";
constexpr auto mimeOutputTab = COPYQ_MIME_PREFIX "output-tab";

#endif // MIMETYPES_H

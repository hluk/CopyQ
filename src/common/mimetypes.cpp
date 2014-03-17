/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

const char mimeText[] = "text/plain";
const char mimeHtml[] = "text/html";
const char mimeUriList[] = "text/uri-list";
const char mimeWindowTitle[] = MIME_PREFIX "owner-window-title";
const char mimeItems[] = MIME_PREFIX "item";
const char mimeItemNotes[] = MIME_PREFIX "item-notes";
const char mimeOwner[] = MIME_PREFIX "owner";
#ifdef COPYQ_WS_X11
const char mimeClipboardMode[] = MIME_PREFIX "clipboard-mode";
#endif

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QLatin1String;

#define COPYQ_MIME_PREFIX "application/x-copyq-"
// Prefix for MIME format not visible to user for data private to plugins.
#define COPYQ_MIME_PRIVATE_PREFIX COPYQ_MIME_PREFIX "private-"
extern const QLatin1String mimePrivatePrefix;
extern const QLatin1String mimeText;
extern const QLatin1String mimeTextUtf8;
extern const QLatin1String mimeHtml;
extern const QLatin1String mimeUriList;
extern const QLatin1String mimeWindowTitle;
extern const QLatin1String mimeItems;
extern const QLatin1String mimeItemNotes;
extern const QLatin1String mimeIcon;
extern const QLatin1String mimeOwner;
extern const QLatin1String mimeClipboardMode;
extern const QLatin1String mimeCurrentTab;
extern const QLatin1String mimeSelectedItems;
extern const QLatin1String mimeCurrentItem;
extern const QLatin1String mimeHidden;
extern const QLatin1String mimeShortcut;
extern const QLatin1String mimeColor;
extern const QLatin1String mimeOutputTab;
extern const QLatin1String mimeDisplayItemInMenu;

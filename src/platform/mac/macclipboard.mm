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

#include "macclipboard.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "common/textdata.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTextCodec>

#include "mactimer.h"

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

MacClipboard::MacClipboard():
        m_prevChangeCount(0)
        , m_clipboardCheckTimer(new MacTimer(this)) {

    m_clipboardCheckTimer->setInterval(250);
    m_clipboardCheckTimer->setTolerance(500);

    connect(m_clipboardCheckTimer, SIGNAL(timeout()), this, SLOT(clipboardTimeout()));

    m_clipboardCheckTimer->start();
}

QVariantMap MacClipboard::data(Mode mode, const QStringList &formats) const {
    Q_UNUSED(mode);

    // On OS X, when you copy files in Finder, etc. you get:
    // - The file name(s) (not paths) as plain text
    // - The file URI(s)
    // - The icon (not thumbnail) for the type of item you have in various image formants
    // We really only want the URI list, so throw the rest away

    QStringList macFormats = QStringList(formats); // Copy so we can modify
    if (mode == PlatformClipboard::Clipboard) {
        const QMimeData *data = clipboardData(QClipboard::Clipboard);

        if (data && data->formats().contains(mimeUriList) && macFormats.contains(mimeUriList)) {
            macFormats = QStringList() << mimeUriList;
        }
    }

    return DummyClipboard::data(mode, macFormats);
}

void MacClipboard::setData(PlatformClipboard::Mode mode, const QVariantMap &dataMap)
{
    auto dataMapForMac = dataMap;

    // Qt adds BOM to UTF-16 text but some application on OS X
    // doesn't expect BOM to be present.
    // This converts text to UTF-16 without BOM.
    const auto text = getTextData(dataMap);
    if ( !text.isEmpty() ) {
        auto codec = QTextCodec::codecForName("UTF-16");
        Q_ASSERT(codec != nullptr);
        if (codec) {
            auto encoder = codec->makeEncoder(QTextCodec::IgnoreHeader);
            dataMapForMac["public.utf16-plain-text"] = encoder->fromUnicode(text);
        }
    }

    return DummyClipboard::setData(mode, dataMapForMac);
}


void MacClipboard::clipboardTimeout() {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger newCount = [pasteboard changeCount];

    if (newCount != m_prevChangeCount) {
        m_prevChangeCount = newCount;
        emit changed(PlatformClipboard::Clipboard);
    }
}

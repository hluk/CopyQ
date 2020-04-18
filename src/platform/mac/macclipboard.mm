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

#include "macclipboard.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QApplication>
#include <QMimeData>
#include <QTextCodec>

#include "mactimer.h"

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

void MacClipboard::startMonitoring(const QStringList &)
{
    auto timer = new MacTimer(this);
    timer->setInterval(250);
    timer->setTolerance(500);
    connect(timer, &MacTimer::timeout, this, &MacClipboard::clipboardTimeout);
    timer->start();
}

void MacClipboard::setData(ClipboardMode mode, const QVariantMap &dataMap)
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

QByteArray MacClipboard::clipboardOwner()
{
    PlatformWindowPtr currentWindow = platformNativeInterface()->getCurrentWindow();
    if (currentWindow)
        return currentWindow->getTitle().toUtf8();
    return QByteArray();
}

void MacClipboard::clipboardTimeout() {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger newCount = [pasteboard changeCount];

    if (newCount != m_prevChangeCount) {
        m_prevChangeCount = newCount;
        emit changed(ClipboardMode::Clipboard);
    }
}

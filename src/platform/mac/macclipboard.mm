// SPDX-License-Identifier: GPL-3.0-or-later

#include "macclipboard.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTextCodec>

#include "mactimer.h"

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

void MacClipboard::startMonitoring(const QStringList &formats)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    m_prevChangeCount = [pasteboard changeCount];

    auto timer = new MacTimer(this);
    timer->setInterval(250);
    timer->setTolerance(500);
    connect(timer, &MacTimer::timeout, this, &MacClipboard::clipboardTimeout);
    timer->start();

    DummyClipboard::startMonitoring(formats);
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

bool MacClipboard::isHidden(const QMimeData &data) const
{
    return data.hasFormat( QStringLiteral("application/x-nspasteboard-concealed-type") );
}

void MacClipboard::onChanged(int mode)
{
    if (mode != QClipboard::Clipboard)
        return;

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger newCount = [pasteboard changeCount];

    if (newCount == m_prevChangeCount)
        return;

    m_prevChangeCount = newCount;

    emit changed(ClipboardMode::Clipboard);
}

void MacClipboard::clipboardTimeout() {
    onChanged(QClipboard::Clipboard);
}

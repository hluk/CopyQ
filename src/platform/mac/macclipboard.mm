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

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#   include <QStringEncoder>
#else
#   include <QTextCodec>
#endif

#include "mactimer.h"

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

void MacClipboard::startMonitoringBackend(const QStringList &formats, ClipboardModeMask modes)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    m_prevChangeCount = [pasteboard changeCount];

    m_timer = new MacTimer(this);
    m_timer->setInterval(250);
    m_timer->setTolerance(500);
    connect(m_timer, &MacTimer::timeout, this, &MacClipboard::clipboardTimeout);
    m_timer->start();

    DummyClipboard::startMonitoringBackend(formats, modes);
}

void MacClipboard::stopMonitoringBackend()
{
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    DummyClipboard::stopMonitoringBackend();
}

void MacClipboard::setData(ClipboardMode mode, const QVariantMap &dataMap)
{
    auto dataMapForMac = dataMap;

    // Qt adds BOM to UTF-16 text but some application on OS X
    // doesn't expect BOM to be present.
    // This converts text to UTF-16 without BOM.
    const auto text = getTextData(dataMap);
    if ( !text.isEmpty() ) {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        auto encoder = QStringEncoder(QStringConverter::Utf16);
        const QByteArray data = encoder.encode(text);
        dataMapForMac[QStringLiteral("public.utf16-plain-text")] = data;
#else
        auto codec = QTextCodec::codecForName(QStringLiteral("UTF-16"));
        Q_ASSERT(codec != nullptr);
        if (codec) {
            auto encoder = codec->makeEncoder(QTextCodec::IgnoreHeader);
            dataMapForMac[QStringLiteral("public.utf16-plain-text")] = encoder->fromUnicode(text);
        }
#endif
    }

    DummyClipboard::setData(mode, dataMapForMac);
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

    emitConnectionChanged(ClipboardMode::Clipboard);
}

void MacClipboard::clipboardTimeout() {
    onChanged(QClipboard::Clipboard);
}

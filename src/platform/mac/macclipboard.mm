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

#include <QStringEncoder>

#include "mactimer.h"

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

namespace {

int clipboardMonitorIntervalMs()
{
    bool ok = false;
    const int ms = qEnvironmentVariableIntValue("COPYQ_CLIPBOARD_MONITOR_INTERVAL_MS", &ok);
    return (ok && ms > 0) ? ms : 500;
}

int clipboardMonitorToleranceMs()
{
    bool ok = false;
    const int ms = qEnvironmentVariableIntValue("COPYQ_CLIPBOARD_MONITOR_TOLERANCE_MS", &ok);
    return (ok && ms >= 0) ? ms : 5000;
}

} // namespace

void MacClipboard::startMonitoringBackend(const QStringList &formats, ClipboardModeMask modes)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    m_prevChangeCount = [pasteboard changeCount];

    m_timer = new MacTimer(this);
    m_timer->setInterval(clipboardMonitorIntervalMs());
    m_timer->setTolerance(clipboardMonitorToleranceMs());
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
        auto encoder = QStringEncoder(QStringConverter::Utf16);
        const QByteArray data = encoder.encode(text);
        dataMapForMac[QStringLiteral("public.utf16-plain-text")] = data;
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
    m_timer->stop();
    onChanged(QClipboard::Clipboard);
    if (m_timer)
        m_timer->start();
}

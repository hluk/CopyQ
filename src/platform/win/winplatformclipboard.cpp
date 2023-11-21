// SPDX-License-Identifier: GPL-3.0-or-later

#include "winplatformclipboard.h"

#include <QClipboard>
#include <QMimeData>
#include <QTimer>

void WinPlatformClipboard::startMonitoring(const QStringList &formats)
{
    m_lastClipboardSequenceNumber = GetClipboardSequenceNumber();

    /* Clipboard needs to be checked in intervals since
     * the QClipboard::changed() signal is not emitted in some cases on Windows.
     */
    QTimer *timer = new QTimer(this);
    timer->setInterval(500);
    connect( timer, &QTimer::timeout, this, [this](){
        onClipboardChanged(QClipboard::Clipboard);
    });
    timer->start();

    DummyClipboard::startMonitoring(formats);
}

bool WinPlatformClipboard::isHidden(const QMimeData &data) const
{
    // QMimeData::hasFormat does not seem to work in this case (in Qt 5.15.2).
    return data.formats().contains(
        QStringLiteral("application/x-qt-windows-mime;value=\"Clipboard Viewer Ignore\""));
}

void WinPlatformClipboard::onChanged(int)
{
    const DWORD newClipboardSequenceNumber = GetClipboardSequenceNumber();
    if (newClipboardSequenceNumber == m_lastClipboardSequenceNumber)
        return;

    m_lastClipboardSequenceNumber = newClipboardSequenceNumber;

    emit changed(ClipboardMode::Clipboard);
}

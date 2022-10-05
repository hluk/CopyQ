// SPDX-License-Identifier: GPL-3.0-or-later

#include "winplatformclipboard.h"

#include <QTimer>

void WinPlatformClipboard::startMonitoring(const QStringList &)
{
    /* Clipboard needs to be checked in intervals since
     * the QClipboard::changed() signal is not emitted in some cases on Windows.
     */
    QTimer *timer = new QTimer(this);
    timer->setInterval(200);
    connect( timer, &QTimer::timeout, this, &WinPlatformClipboard::checkClipboard );
    timer->start();
}

void WinPlatformClipboard::checkClipboard()
{
    const DWORD newClipboardSequenceNumber = GetClipboardSequenceNumber();
    if (newClipboardSequenceNumber == m_lastClipboardSequenceNumber)
        return;

    m_lastClipboardSequenceNumber = newClipboardSequenceNumber;
    emit changed(ClipboardMode::Clipboard);
}

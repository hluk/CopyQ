// SPDX-License-Identifier: GPL-3.0-or-later

#include "winplatformclipboard.h"

#include <QClipboard>
#include <QMimeData>
#include <QTimer>

namespace {

bool contains(const QStringList &formats, const QMimeData &data, const QString &format, const QByteArray &value)
{
    return formats.contains(format) && data.data(format) == value;
}

} // namespace

void WinPlatformClipboard::startMonitoringBackend(const QStringList &formats, ClipboardModeMask modes)
{
    Q_UNUSED(modes)
    m_lastClipboardSequenceNumber = GetClipboardSequenceNumber();

    /* Clipboard needs to be checked in intervals since
     * the QClipboard::changed() signal is not emitted in some cases on Windows.
     */
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect( m_timer, &QTimer::timeout, this, [this](){
        onClipboardChanged(QClipboard::Clipboard);
    });
    m_timer->start();

    DummyClipboard::startMonitoringBackend(formats, modes);
}

void WinPlatformClipboard::stopMonitoringBackend()
{
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    DummyClipboard::stopMonitoringBackend();
}

bool WinPlatformClipboard::isHidden(const QMimeData &data) const
{
    static const QByteArray zeroDWord(4, '\0');

    // QMimeData::hasFormat does not seem to work in this case (in Qt 5.15.2).
    const QStringList formats = data.formats();

    return formats.contains(
            QLatin1String("application/x-qt-windows-mime;value=\"Clipboard Viewer Ignore\""))

        // https://learn.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats#cloud-clipboard-and-clipboard-history-formats
        || formats.contains(
            QLatin1String("application/x-qt-windows-mime;value=\"ExcludeClipboardContentFromMonitorProcessing\""))
        || contains(formats, data,
            QStringLiteral("application/x-qt-windows-mime;value=\"CanIncludeInClipboardHistory\""), zeroDWord)
        || contains(formats, data,
            QStringLiteral("application/x-qt-windows-mime;value=\"CanUploadToCloudClipboard\""), zeroDWord)

        // This is set by some Qt apps
        || contains(formats, data,
            QStringLiteral("ExcludeClipboardContentFromMonitorProcessing"), QByteArrayLiteral("1"));
}

void WinPlatformClipboard::onChanged(int)
{
    const DWORD newClipboardSequenceNumber = GetClipboardSequenceNumber();
    if (newClipboardSequenceNumber == m_lastClipboardSequenceNumber)
        return;

    m_lastClipboardSequenceNumber = newClipboardSequenceNumber;

    emitConnectionChanged(ClipboardMode::Clipboard);
}

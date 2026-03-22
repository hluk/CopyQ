// SPDX-License-Identifier: GPL-3.0-or-later

#include "winplatformclipboard.h"

#include <QClipboard>
#include <QMimeData>

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

    // Clipboard changes are detected via QClipboard::changed signal
    // (Qt 6 uses AddClipboardFormatListener / WM_CLIPBOARDUPDATE).
    DummyClipboard::startMonitoringBackend(formats, modes);
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

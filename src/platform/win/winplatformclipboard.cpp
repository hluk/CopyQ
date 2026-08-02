// SPDX-License-Identifier: GPL-3.0-or-later

#include "winplatformclipboard.h"

#include <QClipboard>
#include <QMimeData>

namespace {

bool contains(const QStringList &formats, const QMimeData &data, const QString &format, const QByteArray &value)
{
    return formats.contains(format) && data.data(format) == value;
}

enum class NativeClipboardState {
    Unavailable,
    Empty,
    HasFormats,
    Unknown,
};

NativeClipboardState nativeClipboardState()
{
    if (!OpenClipboard(nullptr))
        return NativeClipboardState::Unavailable;

    SetLastError(ERROR_SUCCESS);
    const int formatCount = CountClipboardFormats();
    const DWORD countError = GetLastError();
    const bool closed = CloseClipboard();
    if (!closed || (formatCount == 0 && countError != ERROR_SUCCESS))
        return NativeClipboardState::Unknown;

    return formatCount > 0 ? NativeClipboardState::HasFormats
                           : NativeClipboardState::Empty;
}

} // namespace

ClipboardReadResult WinPlatformClipboard::readData(
        ClipboardMode mode, const QStringList &formats) const
{
    const NativeClipboardState state = nativeClipboardState();
    if (state == NativeClipboardState::Unavailable
        || state == NativeClipboardState::Unknown)
    {
        return {};
    }

    ClipboardReadResult result = DummyClipboard::readData(mode, formats);

    // QWindowsClipboardRetrievalMimeData can successfully enumerate formats,
    // then fail to reacquire the IDataObject while materializing the data.  A
    // native object with formats and no Qt formats is the same transient read
    // failure at an earlier point in that sequence, not an empty clipboard.
    if (state == NativeClipboardState::HasFormats
        && result.availableFormats.isEmpty())
    {
        result.isComplete = false;
    }

    return result;
}

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

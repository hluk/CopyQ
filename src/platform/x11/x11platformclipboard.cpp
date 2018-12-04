/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include <QApplication>

#include "x11platformclipboard.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "common/log.h"
#include "common/timer.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <QClipboard>
#include <QMimeData>
#include <QX11Info>

namespace {

constexpr auto minCheckAgainIntervalMs = 50;
constexpr auto maxCheckAgainIntervalMs = 500;

/// Return true only if selection is incomplete, i.e. mouse button or shift key is pressed.
bool isSelectionIncomplete()
{
    if (!QX11Info::isPlatformX11())
        return false;

    auto display = QX11Info::display();

    // If mouse button or shift is pressed then assume that user is selecting text.
    XEvent event{};
    XQueryPointer(display, DefaultRootWindow(display),
                  &event.xbutton.root, &event.xbutton.window,
                  &event.xbutton.x_root, &event.xbutton.y_root,
                  &event.xbutton.x, &event.xbutton.y,
                  &event.xbutton.state);

    return event.xbutton.state & (Button1Mask | ShiftMask);
}

} // namespace

X11PlatformClipboard::X11PlatformClipboard()
{
    // Always assume that only plain text can be in primary selection buffer.
    // Asking a app for bigger data when mouse selection changes can make the app hang for a moment.
    m_selectionData.formats.append(mimeText);

    initSingleShotTimer( &m_timerCheckAgain, 0, this, &X11PlatformClipboard::check );

    initSingleShotTimer( &m_clipboardData.timerEmitChange, 0, this, [this](){
        m_clipboardData.data = m_clipboardData.newData;
        m_clipboardData.newData.clear();
        emit changed(ClipboardMode::Clipboard);
    } );

    initSingleShotTimer( &m_selectionData.timerEmitChange, 0, this, [this](){
        if ( isSelectionIncomplete() ) {
            COPYQ_LOG("Selection is incomplete");
            if ( !m_timerCheckAgain.isActive() )
                m_timerCheckAgain.start(minCheckAgainIntervalMs);
            return;
        }

        m_selectionData.data = m_selectionData.newData;
        m_selectionData.newData.clear();
        emit changed(ClipboardMode::Selection);
    } );
}

void X11PlatformClipboard::setFormats(const QStringList &formats)
{
    m_clipboardData.formats = formats;
    m_clipboardData.data = DummyClipboard::data(ClipboardMode::Clipboard, m_clipboardData.formats);
    m_selectionData.data = DummyClipboard::data(ClipboardMode::Selection, m_selectionData.formats);
}

QVariantMap X11PlatformClipboard::data(ClipboardMode mode, const QStringList &) const
{
    const auto &clipboardData = mode == ClipboardMode::Clipboard ? m_clipboardData : m_selectionData;
    if ( clipboardData.data.contains(mimeOwner) )
        return clipboardData.data;

    auto data = clipboardData.data;
    data[mimeWindowTitle] = clipboardData.owner;
    return data;
}

void X11PlatformClipboard::setData(ClipboardMode mode, const QVariantMap &dataMap)
{
    // WORKAROUND: Avoid getting X11 warning "QXcbClipboard: SelectionRequest too old".
    QCoreApplication::processEvents();
    DummyClipboard::setData(mode, dataMap);
}

void X11PlatformClipboard::onChanged(int mode)
{
    // Store the current window title right after the clipboard/selection changes.
    // This makes sure that the title points to the correct clipboard/selection
    // owner most of the times.
    PlatformPtr platform = createPlatformNativeInterface();
    PlatformWindowPtr currentWindow = platform->getCurrentWindow();
    auto &owner = mode == QClipboard::Clipboard ? m_clipboardData.owner : m_selectionData.owner;
    if (currentWindow)
        owner = currentWindow->getTitle().toUtf8();

    // Omit checking selection too fast.
    if ( mode == QClipboard::Selection && m_timerCheckAgain.isActive() ) {
        COPYQ_LOG("Postponing fast selection change");
        m_selectionData.timerEmitChange.stop();
        return;
    }

    check();
}

void X11PlatformClipboard::check()
{
    m_clipboardData.timerEmitChange.stop();
    m_selectionData.timerEmitChange.stop();
    m_timerCheckAgain.stop();

    const auto changed =
        // Prioritize checking clipboard before selection.
        updateClipboardData(&m_clipboardData, ClipboardMode::Clipboard)
        || updateClipboardData(&m_selectionData, ClipboardMode::Selection);

    // Check clipboard and selection again if some signals where
    // not delivered or older data was received after new one.
    checkAgainLater(changed);
}

bool X11PlatformClipboard::updateClipboardData(X11PlatformClipboard::ClipboardData *clipboardData, ClipboardMode mode)
{
    const auto data = ::clipboardData(mode);
    if (!data)
        return false;

    const auto newDataTimestamp = data->data(QLatin1String("TIMESTAMP"));
    if ( !newDataTimestamp.isEmpty() && clipboardData->newDataTimestamp == newDataTimestamp )
        return false;

    clipboardData->newDataTimestamp = newDataTimestamp;
    clipboardData->newData = DummyClipboard::data(mode, clipboardData->formats);

    if (clipboardData->data == clipboardData->newData)
        return false;

    clipboardData->timerEmitChange.start();
    return true;
}

void X11PlatformClipboard::checkAgainLater(bool clipboardChanged)
{
    const int interval = m_timerCheckAgain.interval() * 2 + minCheckAgainIntervalMs;
    m_timerCheckAgain.setInterval(interval);
    if (interval < maxCheckAgainIntervalMs)
        m_timerCheckAgain.start();
    else if (clipboardChanged)
        m_timerCheckAgain.start(maxCheckAgainIntervalMs);
    else
        m_timerCheckAgain.setInterval(0);

    COPYQ_LOG( QString("Clipboard %1, selection %2.%3")
               .arg(m_clipboardData.data == m_clipboardData.newData ? "unchanged" : "*CHANGED*")
               .arg(m_selectionData.data == m_selectionData.newData ? "unchanged" : "*CHANGED*")
               .arg(m_timerCheckAgain.isActive()
                    ? QString(" Test clipboard in %1ms.").arg(m_timerCheckAgain.interval())
                    : QString())
             );
}

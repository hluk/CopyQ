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

#include "x11displayguard.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <QClipboard>

namespace {

/// Return true only if selection is incomplete, i.e. mouse button or shift key is pressed.
bool isSelectionIncomplete(Display *display)
{
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

X11PlatformClipboard::X11PlatformClipboard(const std::shared_ptr<X11DisplayGuard> &d)
    : d(d)
{
    initSingleShotTimer( &m_timerCheckClipboard, 50, this, SLOT(onClipboardChanged()) );
    initSingleShotTimer( &m_timerCheckSelection, 100, this, SLOT(onSelectionChanged()) );
}

void X11PlatformClipboard::setFormats(const QStringList &formats)
{
    m_formats = formats;
}

QVariantMap X11PlatformClipboard::data(ClipboardMode mode, const QStringList &) const
{
    return mode == ClipboardMode::Clipboard ? m_clipboardData : m_selectionData;
}

void X11PlatformClipboard::setData(ClipboardMode mode, const QVariantMap &dataMap)
{
    DummyClipboard::setData(mode, dataMap);
}

void X11PlatformClipboard::onChanged(int mode)
{
    // Omit checking clipboard and selection too fast.
    if (mode == QClipboard::Clipboard)
        m_timerCheckClipboard.start();
    else
        m_timerCheckSelection.start();
}

void X11PlatformClipboard::onClipboardChanged()
{
    const QVariantMap data = DummyClipboard::data(ClipboardMode::Clipboard, m_formats);
    if (m_clipboardData == data)
        return;

    m_clipboardData = data;
    emit changed(ClipboardMode::Clipboard);

    // Check selection too if some signals where not delivered.
    m_timerCheckSelection.start();
}

void X11PlatformClipboard::onSelectionChanged()
{
    // Qt clipboard data can be in invalid state after this call.
    if ( waitIfSelectionIncomplete() )
        return;

    // Always assume that only plain text can be in primary selection buffer.
    // Asking a app for bigger data when mouse selection changes can make the app hang for a moment.
    const QVariantMap data = DummyClipboard::data( ClipboardMode::Selection, QStringList(mimeText) );
    if (m_selectionData == data)
        return;

    m_selectionData = data;
    emit changed(ClipboardMode::Selection);

    // Check clipboard too if some signals where not delivered.
    m_timerCheckClipboard.start();
}

bool X11PlatformClipboard::waitIfSelectionIncomplete()
{
    if (!d->display())
        return true;

    if ( isSelectionIncomplete(d->display()) ) {
        m_timerCheckSelection.start();
        return true;
    }

    return false;
}

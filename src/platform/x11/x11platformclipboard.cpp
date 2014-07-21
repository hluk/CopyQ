/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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
#include <QClipboard>

#include "x11platformclipboard.h"

#include "x11displayguard.h"

#include "common/common.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

namespace {

/// Return true only if selection is incomplete, i.e. mouse button or shift key is pressed.
bool isSelectionIncomplete(Display *display)
{
    // If mouse button or shift is pressed then assume that user is selecting text.
    XEvent event;
    XQueryPointer(display, DefaultRootWindow(display),
                  &event.xbutton.root, &event.xbutton.window,
                  &event.xbutton.x_root, &event.xbutton.y_root,
                  &event.xbutton.x, &event.xbutton.y,
                  &event.xbutton.state);

    return event.xbutton.state & (Button1Mask | ShiftMask);
}

} // namespace

X11PlatformClipboard::X11PlatformClipboard(const QSharedPointer<X11DisplayGuard> &d)
    : d(d)
    , m_copyclip(false)
    , m_checksel(false)
    , m_copysel(false)
    , m_lastChangedIsClipboard(true)
{
    Q_ASSERT(d->display());

    m_timerIncompleteSelection.setSingleShot(true);
    m_timerIncompleteSelection.setInterval(100);
    connect( &m_timerIncompleteSelection, SIGNAL(timeout()),
             this, SLOT(checkSelectionComplete()) );
}

void X11PlatformClipboard::loadSettings(const QVariantMap &settings)
{
    m_copyclip = settings.value("copy_clipboard", m_copyclip).toBool();
    m_copysel = settings.value("copy_selection", m_copysel).toBool();
    m_checksel = settings.value("check_selection", m_checksel).toBool();
}

QVariantMap X11PlatformClipboard::data(const QStringList &formats) const
{
    return DummyClipboard::data(
                m_lastChangedIsClipboard ? QClipboard::Clipboard : QClipboard::Selection, formats);
}

void X11PlatformClipboard::onChanged(QClipboard::Mode mode)
{
    if ( waitIfSelectionIncomplete() )
        return;

    m_lastChangedIsClipboard = mode == QClipboard::Clipboard;
    emit changed();
}

void X11PlatformClipboard::checkSelectionComplete()
{
    onChanged(QClipboard::Selection);
}

bool X11PlatformClipboard::waitIfSelectionIncomplete()
{
    if ( m_timerIncompleteSelection.isActive() || isSelectionIncomplete(d->display()) ) {
        m_timerIncompleteSelection.start();
        return true;
    }

    return false;
}

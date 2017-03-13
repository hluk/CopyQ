/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

bool isClipboardEmpty(Display *display)
{
    static Atom atom = XInternAtom(display, "CLIPBOARD", False);
    return XGetSelectionOwner(display, atom) == None;
}

bool isSelectionEmpty(Display *display)
{
    static Atom atom = XA_PRIMARY;
    return XGetSelectionOwner(display, atom) == None;
}

} // namespace

X11PlatformClipboard::X11PlatformClipboard(const std::shared_ptr<X11DisplayGuard> &d)
    : d(d)
    , m_resetClipboard(false)
    , m_resetSelection(false)
{
    initSingleShotTimer( &m_timerIncompleteSelection, 100, this, SLOT(checkSelectionComplete()) );
    initSingleShotTimer( &m_timerReset, 500, this, SLOT(resetClipboard()) );
}

void X11PlatformClipboard::loadSettings(const QVariantMap &settings)
{
    m_formats = settings.value("formats", m_formats).toStringList();
}

QVariantMap X11PlatformClipboard::data(Mode mode, const QStringList &) const
{
    return mode == PlatformClipboard::Clipboard ? m_clipboardData : m_selectionData;
}

void X11PlatformClipboard::setData(Mode mode, const QVariantMap &dataMap)
{
    DummyClipboard::setData(mode, dataMap);
}

void X11PlatformClipboard::onChanged(QClipboard::Mode mode)
{
    bool isClip = (mode == QClipboard::Clipboard);
    m_resetClipboard = m_resetClipboard && !isClip;
    m_resetSelection = m_resetSelection && isClip;

    if ( mode == QClipboard::Selection && waitIfSelectionIncomplete() )
        return;

    // Always assume that only plain text can be in primary selection buffer.
    // Asking a app for bigger data when mouse selection changes can make the app hang for a moment.
    QVariantMap data =
            DummyClipboard::data(
                isClip ? Clipboard : Selection,
                isClip ? m_formats : QStringList(mimeText));
    bool foreignData = !ownsClipboardData(data);

    if ( foreignData && maybeResetClipboard(mode) )
        return;

    QVariantMap &targetData = isClip ? m_clipboardData : m_selectionData;
    targetData = data;

    emit changed(isClip ? Clipboard : Selection);
}

void X11PlatformClipboard::checkSelectionComplete()
{
    onChanged(QClipboard::Selection);
}

void X11PlatformClipboard::resetClipboard()
{
    if (m_resetSelection && !m_selectionData.isEmpty()) {
        COPYQ_LOG("Resetting selection");
        DummyClipboard::setData(Selection, m_selectionData);
        m_resetSelection = false;
    }

    if (m_resetClipboard && !m_clipboardData.isEmpty()) {
        COPYQ_LOG("Resetting clipboard");
        DummyClipboard::setData(Clipboard, m_clipboardData);
        m_resetClipboard = false;
    }
}

bool X11PlatformClipboard::waitIfSelectionIncomplete()
{
    if (!d->display())
        return true;

    if ( m_timerIncompleteSelection.isActive() || isSelectionIncomplete(d->display()) ) {
        m_timerIncompleteSelection.start();
        return true;
    }

    return false;
}

bool X11PlatformClipboard::maybeResetClipboard(QClipboard::Mode mode)
{
    if (!d->display())
        return false;

    bool isClip = (mode == QClipboard::Clipboard);
    bool isEmpty = isClip
            ? isClipboardEmpty(d->display())
            : isSelectionEmpty(d->display());

    QVariantMap &clipData = isClip ? m_clipboardData : m_selectionData;

    bool &reset = isClip ? m_resetClipboard : m_resetSelection;
    reset = isEmpty && !clipData.isEmpty();

    // No need reset?
    if (!reset)
        return false;

    COPYQ_LOG( QString("%1 is empty").arg(isClip ? "Clipboard" : "Selection") );

    m_timerReset.start();

    return true;
}

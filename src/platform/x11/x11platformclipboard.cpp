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

#include "x11platformclipboard.h"

#include "x11displayguard.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "common/log.h"

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

X11PlatformClipboard::X11PlatformClipboard(const QSharedPointer<X11DisplayGuard> &d)
    : d(d)
    , m_copyclip(false)
    , m_checksel(false)
    , m_copysel(false)
    , m_resetClipboard(false)
    , m_resetSelection(false)
    , m_syncFromClipboard(false)
{
    Q_ASSERT(d->display());

    initSingleShotTimer( &m_timerIncompleteSelection, 100, this, SLOT(checkSelectionComplete()) );
    initSingleShotTimer( &m_timerReset, 500, this, SLOT(resetClipboard()) );
    initSingleShotTimer( &m_timerSync, 100, this, SLOT(synchronize()) );
}

void X11PlatformClipboard::loadSettings(const QVariantMap &settings)
{
    m_copyclip = settings.value("copy_clipboard", m_copyclip).toBool();
    m_copysel = settings.value("copy_selection", m_copysel).toBool();
    m_checksel = settings.value("check_selection", m_checksel).toBool();
    m_formats = settings.value("formats", m_formats).toStringList();
}

QVariantMap X11PlatformClipboard::data(Mode mode, const QStringList &) const
{
    return mode == PlatformClipboard::Clipboard ? m_clipboardData : m_selectionData;
}

void X11PlatformClipboard::setData(Mode mode, const QVariantMap &dataMap)
{
    DummyClipboard::setData(mode, dataMap);
    if (m_copyclip && mode == Clipboard)
        DummyClipboard::setData(Selection, dataMap);
    else if (m_copysel && mode == Selection)
        DummyClipboard::setData(Clipboard, dataMap);
}

void X11PlatformClipboard::ignoreCurrentData()
{
    COPYQ_LOG("Ignoring clipboard and selection");

    m_timerSync.stop();
    m_timerReset.stop();

    m_resetClipboard = false;
    m_resetSelection = false;

    const QByteArray sessionName = qgetenv("COPYQ_SESSION_NAME");

    if ( m_clipboardData.value(mimeOwner) == sessionName )
        DummyClipboard::setData(PlatformClipboard::Clipboard, QVariantMap());

    if ( m_selectionData.value(mimeOwner) == sessionName )
        DummyClipboard::setData(PlatformClipboard::Selection, QVariantMap());

    m_clipboardData.clear();
    m_selectionData.clear();
}

void X11PlatformClipboard::onChanged(QClipboard::Mode mode)
{
    bool isClip = (mode == QClipboard::Clipboard);
    m_resetClipboard = m_resetClipboard && !isClip;
    m_resetSelection = m_resetSelection && isClip;
    m_timerSync.stop();

    if ( mode == QClipboard::Selection && waitIfSelectionIncomplete() )
        return;

    QVariantMap data = DummyClipboard::data(isClip ? Clipboard : Selection, m_formats);
    bool foreignData = !ownsClipboardData(data);

    if ( foreignData && maybeResetClipboard(mode) )
        return;

    QVariantMap &targetData = isClip ? m_clipboardData : m_selectionData;
    targetData = data;

    if ( foreignData && ((mode == QClipboard::Clipboard) ? m_copyclip : m_copysel) )
        syncFrom(mode);

    if (!m_checksel && mode == QClipboard::Selection)
        return;

    emit changed(isClip ? Clipboard : Selection);
}

void X11PlatformClipboard::checkSelectionComplete()
{
    onChanged(QClipboard::Selection);
}

void X11PlatformClipboard::resetClipboard()
{
    if (m_timerSync.isActive())
        return;

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

void X11PlatformClipboard::synchronize()
{
    if ( isSelectionIncomplete(d->display()) )
        return;

    const QVariantMap &sourceData = m_syncFromClipboard
            ? m_clipboardData
            : m_selectionData;

    if ( !sourceData.isEmpty() ) {
        QClipboard::Mode mode = m_syncFromClipboard ? QClipboard::Selection : QClipboard::Clipboard;
        PlatformClipboard::Mode platformMode = m_syncFromClipboard ? Selection : Clipboard;

        const QMimeData *data = clipboardData(mode);

        if ( !data || sourceData != cloneData(*data, sourceData.keys()) ) {
            COPYQ_LOG( QString("Synchronizing data from %1")
                       .arg(m_syncFromClipboard ? "Clipboard" : "Selection") );
            DummyClipboard::setData(platformMode, sourceData);
        }
    }
}

bool X11PlatformClipboard::waitIfSelectionIncomplete()
{
    if ( m_timerIncompleteSelection.isActive() || isSelectionIncomplete(d->display()) ) {
        m_timerIncompleteSelection.start();
        return true;
    }

    return false;
}

bool X11PlatformClipboard::maybeResetClipboard(QClipboard::Mode mode)
{
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

    reset = !m_timerSync.isActive() || m_syncFromClipboard == (mode == QClipboard::Selection);

    if (reset) {
        m_timerSync.stop();
        m_timerReset.start();
    } else if (m_timerReset.isActive() && !m_resetClipboard && !m_resetSelection) {
        m_timerReset.stop();
    }

    return true;
}

void X11PlatformClipboard::syncFrom(QClipboard::Mode mode)
{
    m_resetClipboard = false;
    m_resetSelection = false;
    m_timerReset.stop();

    m_syncFromClipboard = (mode == QClipboard::Clipboard);
    m_timerSync.start();
}

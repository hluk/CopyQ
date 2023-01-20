// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>

#include "x11platformclipboard.h"

#include "x11info.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "common/log.h"
#include "common/timer.h"

#include "systemclipboard/waylandclipboard.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <QClipboard>
#include <QMimeData>

namespace {

constexpr auto minCheckAgainIntervalMs = 50;
constexpr auto maxCheckAgainIntervalMs = 500;
constexpr auto maxRetryCount = 3;

/// Return true only if selection is incomplete, i.e. mouse button or shift key is pressed.
bool isSelectionIncomplete()
{
    if (!X11Info::isPlatformX11())
        return false;

    auto display = X11Info::display();

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
    m_clipboardData.mode = ClipboardMode::Clipboard;
    m_selectionData.mode = ClipboardMode::Selection;

    // Create Wayland clipboard instance so it can start receiving new data.
    if ( !X11Info::isPlatformX11() ) {
        m_selectionSupported = WaylandClipboard::instance()->isSelectionSupported();
    }
}

void X11PlatformClipboard::startMonitoring(const QStringList &formats)
{
    m_clipboardData.formats = formats;

    // Avoid asking apps for bigger data when mouse selection changes.
    // This could make the app hang for a moment.
    m_selectionData.formats.append(mimeText);
    m_selectionData.formats.append(mimeTextUtf8);
    for (auto &format : formats) {
        if (!format.startsWith(QLatin1String("image/")) && !format.startsWith(QLatin1String("text/")))
            m_selectionData.formats.append(format);
    }

    if ( m_selectionData.enabled && !QGuiApplication::clipboard()->supportsSelection() ) {
        log("X11 selection is not supported, disabling.");
        m_selectionData.enabled = false;
    }

    if ( !X11Info::isPlatformX11() ) {
        connect(WaylandClipboard::instance(), &WaylandClipboard::changed,
                this, [this](QClipboard::Mode mode){ onClipboardChanged(mode); });
    }

    for (auto clipboardData : {&m_clipboardData, &m_selectionData}) {
        clipboardData->owner.clear();
        clipboardData->newOwner.clear();
        updateClipboardData(clipboardData);
        useNewClipboardData(clipboardData);
    }

    initSingleShotTimer( &m_timerCheckAgain, 0, this, &X11PlatformClipboard::check );

    initSingleShotTimer( &m_clipboardData.timerEmitChange, 0, this, [this](){
        useNewClipboardData(&m_clipboardData);
    } );

    initSingleShotTimer( &m_selectionData.timerEmitChange, 0, this, [this](){
        useNewClipboardData(&m_selectionData);
    } );

    DummyClipboard::startMonitoring(formats);

    m_monitoring = true;
}

void X11PlatformClipboard::setMonitoringEnabled(ClipboardMode mode, bool enable)
{
    auto &clipboardData = mode == ClipboardMode::Clipboard ? m_clipboardData : m_selectionData;
    clipboardData.enabled = enable;
}

QVariantMap X11PlatformClipboard::data(ClipboardMode mode, const QStringList &formats) const
{
    if (!m_monitoring)
        return DummyClipboard::data(mode, formats);

    const auto &clipboardData = mode == ClipboardMode::Clipboard ? m_clipboardData : m_selectionData;

    auto data = clipboardData.data;
    if ( !data.contains(mimeOwner) )
        data[mimeWindowTitle] = clipboardData.owner;
    return data;
}

void X11PlatformClipboard::setData(ClipboardMode mode, const QVariantMap &dataMap)
{
    if ( X11Info::isPlatformX11() ) {
        // WORKAROUND: Avoid getting X11 warning "QXcbClipboard: SelectionRequest too old".
        QCoreApplication::processEvents();
        DummyClipboard::setData(mode, dataMap);
    } else {
        const auto data = createMimeData(dataMap);
        const auto qmode = modeToQClipboardMode(mode);
        WaylandClipboard::instance()->setMimeData(data, qmode);

        // This makes pasting the clipboard work in own widgets.
        const auto data2 = createMimeData(dataMap);
        QGuiApplication::clipboard()->setMimeData(data2, qmode);
    }
}

const QMimeData *X11PlatformClipboard::mimeData(ClipboardMode mode) const
{
    if ( X11Info::isPlatformX11() )
        return DummyClipboard::mimeData(mode);

    return WaylandClipboard::instance()->mimeData( modeToQClipboardMode(mode) );
}

void X11PlatformClipboard::onChanged(int mode)
{
    auto &clipboardData = mode == QClipboard::Clipboard ? m_clipboardData : m_selectionData;
    if (!clipboardData.enabled)
        return;

    // Store the current window title right after the clipboard/selection changes.
    // This makes sure that the title points to the correct clipboard/selection
    // owner most of the times.
    const auto currentWindowTitle = clipboardOwner();
    if (currentWindowTitle != clipboardData.newOwner) {
        COPYQ_LOG( QString("New %1 owner: \"%2\"")
                   .arg(
                       QString::fromLatin1(mode == QClipboard::Clipboard ? "clipboard" : "selection"),
                       QString::fromUtf8(currentWindowTitle)
                   ) );
        clipboardData.newOwner = currentWindowTitle;
    }

    if (mode == QClipboard::Selection) {
        // Omit checking selection too fast.
        if ( mode == QClipboard::Selection && m_timerCheckAgain.isActive() ) {
            COPYQ_LOG("Postponing fast selection change");
            return;
        }

        if ( isSelectionIncomplete() ) {
            COPYQ_LOG("Selection is incomplete");
            if ( !m_timerCheckAgain.isActive()
                 || minCheckAgainIntervalMs < m_timerCheckAgain.remainingTime() )
            {
                m_timerCheckAgain.start(minCheckAgainIntervalMs);
            }
            return;
        }
    }

    if (m_clipboardData.cloningData || m_selectionData.cloningData) {
        if (clipboardData.cloningData && !clipboardData.abortCloning) {
            COPYQ_LOG( QString("Aborting getting %1, the data changed again")
                       .arg(mode == QClipboard::Clipboard ? "clipboard" : "selection") );
            clipboardData.abortCloning = true;
        }
        return;
    }

    updateClipboardData(&clipboardData);

    checkAgainLater(true, 0);
}

void X11PlatformClipboard::check()
{
    if (m_clipboardData.cloningData || m_selectionData.cloningData) {
        m_timerCheckAgain.setInterval(minCheckAgainIntervalMs);
        m_timerCheckAgain.start();
        return;
    }

    m_timerCheckAgain.stop();

    updateClipboardData(&m_clipboardData);
    updateClipboardData(&m_selectionData);

    if ( m_timerCheckAgain.isActive() )
        return;

    const bool changed = m_clipboardData.timerEmitChange.isActive()
        || m_selectionData.timerEmitChange.isActive();

    // Check clipboard and selection again if some signals where
    // not delivered or older data was received after new one.
    const int interval = m_timerCheckAgain.interval() * 2 + minCheckAgainIntervalMs;
    checkAgainLater(changed, interval);
}

void X11PlatformClipboard::updateClipboardData(X11PlatformClipboard::ClipboardData *clipboardData)
{
    if (!clipboardData->enabled)
        return;

    if ( isSelectionIncomplete() ) {
        m_timerCheckAgain.start(minCheckAgainIntervalMs);
        return;
    }

    const auto data = mimeData(clipboardData->mode);

    // Retry to retrieve clipboard data few times.
    if (!data) {
        if ( !X11Info::isPlatformX11() )
            return;

        if (clipboardData->retry < maxRetryCount) {
            ++clipboardData->retry;
            m_timerCheckAgain.start(clipboardData->retry * maxCheckAgainIntervalMs);
        }

        log( QString("Failed to retrieve %1 data (try %2/%3)")
             .arg(clipboardData->mode == ClipboardMode::Clipboard ? "clipboard" : "selection")
             .arg(clipboardData->retry)
             .arg(maxRetryCount), LogWarning );

        return;
    }
    clipboardData->retry = 0;

    const QByteArray newDataTimestampData = data->data(QLatin1String("TIMESTAMP"));
    quint32 newDataTimestamp = 0;
    if ( !newDataTimestampData.isEmpty() ) {
        QDataStream stream(newDataTimestampData);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream >> newDataTimestamp;
        if (stream.status() != QDataStream::Ok)
            newDataTimestamp = 0;
    }

    // In case there is a valid timestamp, omit update if the timestamp and
    // text did not change.
    if ( newDataTimestamp != 0 && clipboardData->newDataTimestamp == newDataTimestamp ) {
        const QVariantMap newData = cloneData(*data, {mimeText});
        if (newData.value(mimeText) == clipboardData->newData.value(mimeText))
            return;
    }

    clipboardData->timerEmitChange.stop();
    clipboardData->abortCloning = false;
    clipboardData->cloningData = true;
    clipboardData->newData = cloneData(*data, clipboardData->formats, &clipboardData->abortCloning);
    clipboardData->cloningData = false;
    if (clipboardData->abortCloning) {
        m_timerCheckAgain.setInterval(0);
        m_timerCheckAgain.start();
        return;
    }

    // In case there is no timestamp, update only if the data changed.
    if ( newDataTimestamp == 0 && clipboardData->data == clipboardData->newData )
        return;

    clipboardData->newDataTimestamp = newDataTimestamp;
    clipboardData->timerEmitChange.start();
}

void X11PlatformClipboard::useNewClipboardData(X11PlatformClipboard::ClipboardData *clipboardData)
{
    clipboardData->data = clipboardData->newData;
    clipboardData->owner = clipboardData->newOwner;
    clipboardData->timerEmitChange.stop();
    emit changed(clipboardData->mode);
}

void X11PlatformClipboard::checkAgainLater(bool clipboardChanged, int interval)
{
    m_timerCheckAgain.setInterval(interval);
    if (interval < maxCheckAgainIntervalMs)
        m_timerCheckAgain.start();
    else if (clipboardChanged)
        m_timerCheckAgain.start(maxCheckAgainIntervalMs);
    else
        m_timerCheckAgain.setInterval(0);

    COPYQ_LOG( QString("Clipboard %1, selection %2.%3")
               .arg(
                   QString::fromLatin1(m_clipboardData.timerEmitChange.isActive() ? "*CHANGED*" : "unchanged"),
                   QString::fromLatin1(m_selectionData.timerEmitChange.isActive() ? "*CHANGED*" : "unchanged"),
                   m_timerCheckAgain.isActive()
                    ? QString(" Test again in %1ms.").arg(m_timerCheckAgain.interval())
                    : QString()
               ) );
}

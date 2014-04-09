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

#include "clipboardmonitor.h"

#include "common/arguments.h"
#include "common/client_server.h"
#include "common/common.h"
#include "common/mimetypes.h"
#include "common/monitormessagecode.h"
#include "item/clipboarditem.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QApplication>
#include <QMimeData>
#include <QScopedPointer>
#include <QTimer>

#ifdef COPYQ_WS_X11
#  include "platform/x11/x11platform.h"
#endif

#ifdef Q_OS_MAC
#  include "platform/mac/macplatform.h"
#  include "platform/mac/mactimer.h"
#endif

namespace {

void setClipboardData(const QVariantMap &data, QClipboard::Mode mode)
{
    Q_ASSERT( isMainThread() );
    COPYQ_LOG( QString("Setting %1.").arg(mode == QClipboard::Clipboard ? "clipboard"
                                                                        : "selection") );

    QScopedPointer<QMimeData> mimeData( createMimeData(data) );

#ifdef HAS_TESTS
    // Don't set clipboard owner if monitor is only used to set clipboard for tests.
    if ( !QCoreApplication::instance()->property("CopyQ_testing").toBool() )
#endif
        mimeData->setData(mimeOwner, QByteArray());

    QApplication::clipboard()->setMimeData( mimeData.take(), mode );
}

} // namespace

#ifdef COPYQ_WS_X11
class PrivateX11 {
public:
    PrivateX11()
        : m_x11Platform()
        , m_timer()
        , m_syncTimer()
        , m_resetTimer()
        , m_clipboardData()
        , m_selectionData()
        , m_syncTo(QClipboard::Clipboard)
        , m_resetClipboard(false)
        , m_resetSelection(false)
        , m_copyclip(false)
        , m_checksel(false)
        , m_copysel(false)
    {
        m_timer.setSingleShot(true);
        m_timer.setInterval(100);

        m_syncTimer.setSingleShot(true);
        m_syncTimer.setInterval(100);

        m_resetTimer.setSingleShot(true);
        m_resetTimer.setInterval(500);
    }

    bool waitForKeyRelease()
    {
        if (m_timer.isActive())
            return true;

        if ( m_x11Platform.isSelecting() ) {
            m_timer.start();
            return true;
        }

        return false;
    }

    const QTimer &timer() const
    {
        return m_timer;
    }

    const QTimer &syncTimer() const
    {
        return m_syncTimer;
    }

    const QTimer &resetClipboardTimer() const
    {
        return m_resetTimer;
    }

    bool synchronize(QClipboard::Mode modeSyncTo)
    {
        if ( (modeSyncTo == QClipboard::Selection) ? !m_copyclip : !m_copysel ) {
            m_syncTimer.stop();
            return false;
        }

        m_resetClipboard = false;
        m_resetSelection = false;
        m_resetTimer.stop();

        m_syncTo = modeSyncTo;
        m_syncTimer.start();
        return true;
    }

    void synchronize()
    {
        if (m_syncTimer.isActive())
            return;

        bool isClip = (m_syncTo == QClipboard::Clipboard);
        const QVariantMap &sourceData = isClip ? m_selectionData : m_clipboardData;
        if ( sourceData.isEmpty() )
            return;

        if (m_syncTo == QClipboard::Selection && waitForKeyRelease()) {
            m_syncTimer.start();
            return;
        }

        setClipboardData(sourceData, m_syncTo);
    }

    void cancelSynchronization()
    {
        m_syncTimer.stop();
    }

    /**
     * Remember last non-empty clipboard content and reset clipboard after interval if there is no owner.
     *
     * @return false if clipboard doesn't have given formats, otherwise true
     */
    bool resetClipboard(QClipboard::Mode mode, const QMimeData *data, const QStringList &formats)
    {
        Q_ASSERT(data != NULL);

        bool isClip = (mode == QClipboard::Clipboard);
        bool isEmpty = isClip ? m_x11Platform.isClipboardEmpty() : m_x11Platform.isSelectionEmpty();
        QVariantMap &clipData = isClip ? m_clipboardData : m_selectionData;

        // Need reset?
        if (isEmpty) {
            COPYQ_LOG( QString("%1 is empty").arg(isClip ? "Clipboard" : "Selection") );
            if (clipData.isEmpty())
                return true;

            bool &reset = isClip ? m_resetClipboard : m_resetSelection;
            reset = !m_syncTimer.isActive() || m_syncTo == mode;

            if (reset) {
                m_syncTimer.stop();
                m_resetTimer.start();
            } else if (m_resetTimer.isActive() && !m_resetClipboard && !m_resetSelection) {
                m_resetTimer.stop();
            }

            return false;
        }

        QVariantMap dataCopy( cloneData(*data, formats) );
        if ( dataCopy.isEmpty() )
            return false;

        clipData = dataCopy;
        return true;
    }

    void resetClipboard()
    {
        if (m_resetTimer.isActive() || m_syncTimer.isActive())
            return;

        if (m_resetSelection && !m_selectionData.isEmpty()) {
            COPYQ_LOG( QString("Resetting selection") );
            setClipboardData(m_selectionData, QClipboard::Selection);
        }

        if (m_resetClipboard && !m_clipboardData.isEmpty()) {
            COPYQ_LOG( QString("Resetting clipboard") );
            setClipboardData(m_clipboardData, QClipboard::Clipboard);
        }
    }

    void loadSettings(const QVariantMap &settings)
    {
        if ( settings.contains("copy_clipboard") )
            m_copyclip = settings["copy_clipboard"].toBool();
        if ( settings.contains("copy_selection") )
            m_copysel = settings["copy_selection"].toBool();
        if ( settings.contains("check_selection") )
            m_checksel = settings["check_selection"].toBool();
    }

    bool hasCopyClipboard() const { return m_copyclip; }

    bool hasCheckSelection() const { return m_checksel; }

    bool hasCopySelection() const { return m_copysel; }

    const QVariantMap &data(QClipboard::Mode mode) const
    {
        return mode == QClipboard::Clipboard ? m_clipboardData : m_selectionData;
    }

private:
    X11Platform m_x11Platform;
    QTimer m_timer;
    QTimer m_syncTimer;
    QTimer m_resetTimer;
    QVariantMap m_clipboardData;
    QVariantMap m_selectionData;
    QClipboard::Mode m_syncTo;
    bool m_resetClipboard;
    bool m_resetSelection;

    bool m_copyclip;
    bool m_checksel;
    bool m_copysel;
};
#endif

ClipboardMonitor::ClipboardMonitor(int &argc, char **argv)
    : Client()
    , App(createPlatformNativeInterface()->createMonitorApplication(argc, argv))
    , m_formats()
    , m_newdata()
    , m_updateTimer( new QTimer(this) )
    , m_needCheckClipboard(false)
#ifdef COPYQ_WS_X11
    , m_needCheckSelection(false)
    , m_x11(new PrivateX11)
#endif
#ifdef Q_OS_MAC
    , m_prevChangeCount(0)
    , m_clipboardCheckTimer(new MacTimer(this))
    , m_macPlatform(new MacPlatform())
#endif
{
    Q_ASSERT(argc == 3);
    const QString serverName( QString::fromUtf8(argv[2]) );

#ifdef HAS_TESTS
    if ( serverName == QString("copyq_TEST") )
        QCoreApplication::instance()->setProperty("CopyQ_testing", true);
#endif

    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(300);
    connect( m_updateTimer, SIGNAL(timeout()),
             this, SLOT(updateTimeout()));

#ifdef COPYQ_WS_X11
    connect( &m_x11->timer(), SIGNAL(timeout()),
             this, SLOT(updateSelection()) );
    connect( &m_x11->syncTimer(), SIGNAL(timeout()),
             this, SLOT(synchronize()) );
    connect( &m_x11->resetClipboardTimer(), SIGNAL(timeout()),
             this, SLOT(resetClipboard()) );
#endif

#ifdef Q_OS_MAC
    m_clipboardCheckTimer->setInterval(250);
    m_clipboardCheckTimer->setTolerance(500);
    connect(m_clipboardCheckTimer, SIGNAL(timeout()), this, SLOT(clipboardTimeout()));
    m_clipboardCheckTimer->start();
#endif

    Arguments arguments(argc, argv);
    if ( !startClientSocket(serverName, arguments) )
        exit(1);
}

ClipboardMonitor::~ClipboardMonitor()
{
#ifdef COPYQ_WS_X11
    delete m_x11;
#endif
#ifdef Q_OS_MAC
    delete m_macPlatform;
#endif
}

#ifdef COPYQ_WS_X11
bool ClipboardMonitor::updateSelection(bool check)
{
    // Wait while selection is incomplete, i.e. mouse button or
    // shift key is pressed.
    if ( m_x11->waitForKeyRelease() )
        return false;

    if (check)
        checkClipboard(QClipboard::Selection);
    return true;
}
#endif

#ifdef COPYQ_WS_X11
void ClipboardMonitor::synchronize()
{
    m_x11->synchronize();
}

void ClipboardMonitor::resetClipboard()
{
    m_x11->resetClipboard();
}
#endif /* !COPYQ_WS_X11 */

void ClipboardMonitor::checkClipboard(QClipboard::Mode mode)
{
#ifdef COPYQ_WS_X11
    m_x11->cancelSynchronization();
#endif

    // Check clipboard after interval because someone is updating it very quickly.
    bool needToWait = m_updateTimer->isActive();
    if (mode == QClipboard::Clipboard)
        m_needCheckClipboard = needToWait;
#ifdef COPYQ_WS_X11
    else if (mode == QClipboard::Selection)
        m_needCheckSelection = needToWait;
#endif
    else
        return;

    m_updateTimer->start();
    if (needToWait)
        return;

#ifdef COPYQ_WS_X11
    if (mode == QClipboard::Selection && !updateSelection(false) )
        return;
#endif

    COPYQ_LOG( QString("Checking for new %1 content.")
               .arg(mode == QClipboard::Clipboard ? "clipboard" : "selection") );

    // get clipboard data
    const QMimeData *data = clipboardData(mode);

    // data retrieved?
    if (!data) {
        log( tr("Cannot access clipboard data!"), LogError );
        return;
    }

    // clone only mime types defined by user
#ifdef COPYQ_WS_X11
    if ( !m_x11->resetClipboard(mode, data, m_formats) )
        return; // no owner -> reset content
    QVariantMap data2 = m_x11->data(mode);
#elif defined(Q_OS_MAC)
    //  On OS X, when you copy files in Finder, etc. you get:
    //  - The file name(s) (not paths) as plain text
    //  - The file URI(s)
    //  - The icon (not thumbnail) for the type of item you have in various image formants
    // We really only want the URI list, so throw the rest away
    QStringList formats = m_formats;
    if (data->formats().contains(mimeUriList) && formats.contains(mimeUriList)) {
        formats = QStringList() << mimeUriList;
    }
    QVariantMap data2( cloneData(*data, formats) );
#else
    QVariantMap data2( cloneData(*data, m_formats) );
#endif

    // add window title of clipboard owner
    PlatformPtr platform = createPlatformNativeInterface();
    PlatformWindowPtr currentWindow = platform->getCurrentWindow();
    data2.insert( mimeWindowTitle, currentWindow ? currentWindow->getTitle().toUtf8() : QByteArray() );

#ifdef COPYQ_WS_X11
    if (mode == QClipboard::Clipboard) {
        if ( !ownsClipboardData(data2) && m_x11->synchronize(QClipboard::Selection) )
            m_needCheckSelection = false;
        clipboardChanged(data2);
    } else {
        if (!m_x11->hasCopySelection())
            data2.insert(mimeClipboardMode, "selection");
        if ( !ownsClipboardData(data2) && m_x11->synchronize(QClipboard::Clipboard) )
            m_needCheckClipboard = false;
        if ( m_x11->hasCheckSelection() )
            clipboardChanged(data2);
    }
#else /* !COPYQ_WS_X11 */
    clipboardChanged(data2);
#endif
}

void ClipboardMonitor::clipboardChanged(const QVariantMap &data)
{
    sendMessage( serializeData(data), MonitorClipboardChanged );
}

void ClipboardMonitor::updateTimeout()
{
    if (m_needCheckClipboard) {
        checkClipboard(QClipboard::Clipboard);
#ifdef COPYQ_WS_X11
    } else if (m_needCheckSelection) {
        checkClipboard(QClipboard::Selection);
#endif
    } else if ( !m_newdata.isEmpty() ) {
        updateClipboard();
    }
}

void ClipboardMonitor::onMessageReceived(const QByteArray &message, int messageCode)
{
    if (messageCode == MonitorPing) {
        sendMessage( QByteArray(), MonitorPong );
    } else if (messageCode == MonitorSettings) {
        QDataStream stream(message);
        QVariantMap settings;
        stream >> settings;

        if ( hasLogLevel(LogDebug) ) {
            COPYQ_LOG("Loading configuration:");
            foreach (const QString &key, settings.keys()) {
                QVariant val = settings[key];
                const QString str = val.canConvert<QStringList>() ? val.toStringList().join(",")
                                                                  : val.toString();
                COPYQ_LOG( QString("    %1=%2").arg(key).arg(str) );
            }
        }

        if ( settings.contains("formats") )
            m_formats = settings["formats"].toStringList();
#ifdef COPYQ_WS_X11
        m_x11->loadSettings(settings);
#endif

        connect( QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)),
                 this, SLOT(checkClipboard(QClipboard::Mode)), Qt::UniqueConnection );

#ifdef COPYQ_WS_X11
        checkClipboard(QClipboard::Selection);
#endif
        checkClipboard(QClipboard::Clipboard);

        COPYQ_LOG("Configured");
    } else if (messageCode == MonitorChangeClipboard) {
        QVariantMap data;
        deserializeData(&data, message);
        updateClipboard(data);
    } else {
        log( QString("Unknown message code %1!").arg(messageCode), LogError );
    }
}

void ClipboardMonitor::clipboardTimeout() {
#ifdef Q_OS_MAC
    long int newCount = m_macPlatform->getChangeCount();
    if (newCount != m_prevChangeCount) {
        m_prevChangeCount = newCount;
        checkClipboard(QClipboard::Clipboard);
    }
#endif
}

void ClipboardMonitor::onDisconnected()
{
    exit(0);
}

void ClipboardMonitor::updateClipboard(const QVariantMap &data)
{
    if ( !data.isEmpty() )
        m_newdata = data;
    if ( m_updateTimer->isActive() )
        return;

    COPYQ_LOG("Updating clipboard");

    setClipboardData(m_newdata, QClipboard::Clipboard);
    m_needCheckClipboard = false;

#ifdef COPYQ_WS_X11
    if ( m_x11->hasCopyClipboard() ) {
        setClipboardData(m_newdata, QClipboard::Selection);
        m_needCheckSelection = false;
    }
#endif

    m_newdata.clear();

    m_updateTimer->start();
}

void ClipboardMonitor::exit(int exitCode)
{
    m_updateTimer->start(); // Don't check clipboard after this.
    App::exit(exitCode);
}

void ClipboardMonitor::log(const QString &text, const LogLevel level)
{
    if ( hasLogLevel(level) )
        return;

    const QString message = createLogMessage("Clipboard Monitor", text, level);
    sendMessage( message.toUtf8(), MonitorLog );
}

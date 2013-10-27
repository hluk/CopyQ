/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "common/client_server.h"
#include "common/common.h"
#include "item/clipboarditem.h"
#include "platform/platformnativeinterface.h"

#include <QApplication>
#include <QMimeData>
#include <QScopedPointer>
#include <QTimer>

#ifdef COPYQ_WS_X11
#  include "platform/x11/x11platform.h"
#endif

namespace {

void setClipboardData(const QVariantMap &data, QClipboard::Mode mode)
{
    Q_ASSERT( isMainThread() );
    COPYQ_LOG( QString("Setting %1.").arg(mode == QClipboard::Clipboard ? "clipboard"
                                                                        : "selection") );
    QApplication::clipboard()->setMimeData( createMimeData(data), mode );
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

            bool &reset = isClip ? m_resetClipboard : m_resetSelection;
            reset = !clipData.isEmpty() && (!m_syncTimer.isActive() || m_syncTo == mode);

            if (reset) {
                m_syncTimer.stop();
                m_resetTimer.start();
            } else if (m_resetTimer.isActive() && !m_resetClipboard && !m_resetSelection) {
                m_resetTimer.stop();
            }

            return false;
        }

        QVariantMap dataCopy(cloneData(*data, &formats));
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
    : QObject()
    , App(new QApplication(argc, argv))
    , m_formats()
    , m_newdata()
    , m_socket( new QLocalSocket(this) )
    , m_updateTimer( new QTimer(this) )
    , m_needCheckClipboard(false)
#ifdef COPYQ_WS_X11
    , m_needCheckSelection(false)
    , m_x11(new PrivateX11)
#endif
{
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
    connect( m_socket, SIGNAL(disconnected()),
             this, SLOT(onDisconnected()) );

    QStringList args = QCoreApplication::instance()->arguments();
    Q_ASSERT(args.size() == 3);

    const QString &serverName = args[2];
    m_socket->connectToServer(serverName);
    if ( !m_socket->waitForConnected(2000) )
        exit(1);

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
}

ClipboardMonitor::~ClipboardMonitor()
{
#ifdef COPYQ_WS_X11
    delete m_x11;
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
    // Check clipboard after interval because someone is updating it very quickly.
    bool needToWait = m_updateTimer->isActive();
    if (mode == QClipboard::Clipboard)
        m_needCheckClipboard = needToWait;
#ifdef COPYQ_WS_X11
    else if (mode == QClipboard::Selection)
        m_needCheckSelection = needToWait;
#endif

    m_updateTimer->start();
    if (needToWait)
        return;

#ifdef COPYQ_WS_X11
    if (mode == QClipboard::Clipboard) {
        if ( QApplication::clipboard()->ownsClipboard() )
            return;
    } else if (mode == QClipboard::Selection) {
        if ( QApplication::clipboard()->ownsSelection() || !updateSelection(false) )
            return;
    } else {
        return;
    }
#else /* !COPYQ_WS_X11 */
    // check if clipboard data are needed
    if (mode != QClipboard::Clipboard || QApplication::clipboard()->ownsClipboard())
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
#else
    QVariantMap data2( cloneData(*data, &m_formats) );
#endif

    // add window title of clipboard owner
    PlatformPtr platform = createPlatformNativeInterface();
    data2.insert( mimeWindowTitle,
                  platform->getWindowTitle(platform->getCurrentWindow()).toUtf8() );

#ifdef COPYQ_WS_X11
    if (mode == QClipboard::Clipboard) {
        if ( m_x11->synchronize(QClipboard::Selection) )
            m_needCheckSelection = false;
        clipboardChanged(data2);
    } else {
        if (!m_x11->hasCopySelection())
            data2.insert(mimeClipboardMode, "selection");
        if ( m_x11->synchronize(QClipboard::Clipboard) )
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
    ClipboardItem item;

    item.setData(data);

    // send clipboard item
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << item;
    writeMessage(msg);
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

void ClipboardMonitor::readyRead()
{
    m_socket->blockSignals(true);

    while ( m_socket->bytesAvailable() > 0 ) {
        QByteArray msg;
        if( !readMessage(m_socket, &msg) ) {
            log( tr("Cannot read message from server!"), LogError );
            exit(1);
            return;
        }

        if (msg == "ping") {
            writeMessage(QByteArray("pong") );
        } else {
            ClipboardItem item;
            QDataStream in(&msg, QIODevice::ReadOnly);
            in >> item;

            /* Does server send settings for monitor? */
            QByteArray settingsData = item.data(mimeApplicationSettings);
            if ( !settingsData.isEmpty() ) {
                QDataStream settings_in(settingsData);
                QVariantMap settings;
                settings_in >> settings;

#ifdef COPYQ_LOG_DEBUG
                {
                    COPYQ_LOG("Loading configuration:");
                    foreach (const QString &key, settings.keys()) {
                        QVariant val = settings[key];
                        const QString str = val.canConvert<QStringList>() ? val.toStringList().join(",")
                                                                          : val.toString();
                        COPYQ_LOG( QString("    %1=%2").arg(key).arg(str) );
                    }
                }
#endif

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
            } else {
                updateClipboard(item.data());
            }
        }
    }

    m_socket->blockSignals(false);
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

#ifdef COPYQ_WS_X11
    setClipboardData(m_newdata, QClipboard::Selection);
    m_needCheckSelection = false;
#endif
    setClipboardData(m_newdata, QClipboard::Clipboard);
    m_needCheckClipboard = false;

    m_newdata.clear();

    m_updateTimer->start();
}

void ClipboardMonitor::exit(int exitCode)
{
    m_updateTimer->start(); // Don't check clipboard after this.
    m_socket->abort();
    App::exit(exitCode);
}

void ClipboardMonitor::writeMessage(const QByteArray &msg)
{
    if ( !::writeMessage(m_socket, msg) ) {
        log( "Failed to send data to server!", LogError );
        exit(1);
    }
}

void ClipboardMonitor::log(const QString &text, const LogLevel level)
{
    ClipboardItem item;

    QString message = createLogMessage("Clipboard Monitor", text, level);
    item.setData( mimeMessage, message.trimmed().toUtf8() );

    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << item;

    writeMessage(msg);
}

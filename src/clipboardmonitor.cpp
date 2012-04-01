#include "clipboardmonitor.h"
#include "clipboardserver.h"
#include "configurationmanager.h"
#include "clipboarditem.h"
#include "client_server.h"
#include <QMimeData>

#ifdef Q_WS_X11
#include <QX11Info>
#include <X11/Xlib.h>
#endif

ClipboardMonitor::ClipboardMonitor(int &argc, char **argv) :
    App(argc, argv), m_newdata(NULL)
{
    m_socket = new QLocalSocket(this);
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(readyRead()), Qt::DirectConnection );
    connect( m_socket, SIGNAL(disconnected()),
             this, SLOT(quit()) );
    m_socket->connectToServer( ClipboardServer::monitorServerName() );
    if ( !m_socket->waitForConnected(2000) ) {
        log( tr("Cannot connect to server!"), LogError );
        exit(1);
    }

    ConfigurationManager *cm = ConfigurationManager::instance();
    setFormats( cm->value("formats").toString() );
    setCheckClipboard( cm->value("check_clipboard").toBool() );

    bool ok;
    m_lastHash = cm->value("_last_hash").toUInt(&ok);
    if (!ok)
        m_lastHash = 0;

#ifdef Q_WS_X11
    setCopyClipboard( cm->value("copy_clipboard").toBool() );
    setCheckSelection( cm->value("check_selection").toBool() );
    setCopySelection( cm->value("copy_selection").toBool() );
    m_timer.setSingleShot(true);
    m_timer.setInterval(100);
    connect( &m_timer, SIGNAL(timeout()),
             this, SLOT(updateSelection()) );
#endif

    delete cm;

    m_updatetimer.setSingleShot(true);
    m_updatetimer.setInterval(500);
    connect( &m_updatetimer, SIGNAL(timeout()),
             this, SLOT(updateTimeout()), Qt::DirectConnection);

    connect( QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)),
             this, SLOT(checkClipboard(QClipboard::Mode)) );

#ifdef Q_WS_X11
    checkClipboard(QClipboard::Selection);
#endif
    checkClipboard(QClipboard::Clipboard);
}

void ClipboardMonitor::setFormats(const QString &list)
{
    m_formats = list.split( QRegExp("[;,\\s]+") );
}

#ifdef Q_WS_X11
bool ClipboardMonitor::updateSelection(bool check)
{
    // wait while selection is incomplete, i.e. mouse button or
    // shift key is pressed
    if ( m_timer.isActive() )
        return false;

    XEvent event;

    XQueryPointer(QX11Info::display(), QX11Info::appRootWindow(),
                  &event.xbutton.root, &event.xbutton.window,
                  &event.xbutton.x_root, &event.xbutton.y_root,
                  &event.xbutton.x, &event.xbutton.y,
                  &event.xbutton.state);

    if( event.xbutton.state & (Button1Mask | ShiftMask) ) {
        m_timer.start();
        return false;
    }

    if (check)
        checkClipboard(QClipboard::Selection);

    return true;
}

void ClipboardMonitor::checkClipboard(QClipboard::Mode mode)
{
    const QMimeData *data;
    uint newHash;

    // check if clipboard data are needed
    if (mode == QClipboard::Clipboard) {
        if (!m_checkclip && !m_copyclip)
            return;
    } else if (mode == QClipboard::Selection) {
        if ((!m_checksel && !m_copysel) || !updateSelection(false))
            return;
    } else {
        return;
    }

    // get clipboard data
    data = clipboardData(mode);

    // data retrieved?
    if (!data) {
        log( tr("Cannot access clipboard data!"), LogError );
        return;
    }

    // same data as last time?
    newHash = hash(*data, m_formats);
    if (m_lastHash == newHash)
        return;

    // clone only mime types defined by user
    data = cloneData(*data, &m_formats);
    // any data found?
    if ( data->formats().isEmpty() ) {
        delete data;
        return;
    }

    // send data to serve and synchronize if needed
    m_lastHash = newHash;
    if (mode == QClipboard::Clipboard) {
        if (m_checkclip)
            clipboardChanged(mode, cloneData(*data));
        if (m_copyclip)
            setClipboardData( cloneData(*data), QClipboard::Selection );
    } else {
        if (m_checksel)
            clipboardChanged(mode, cloneData(*data));
        if (m_copysel)
            setClipboardData( cloneData(*data), QClipboard::Clipboard );
    }

    delete data;
}
#else /* !Q_WS_X11 */
void ClipboardMonitor::checkClipboard(QClipboard::Mode mode)
{
    const QMimeData *data;
    QMimeData *data2;
    uint newHash;

    // check if clipboard data are needed
    if (mode != QClipboard::Clipboard || !m_checkclip)
        return;

    // get clipboard data
    data = clipboardData(mode);

    // data retrieved?
    if (!data) {
        log( tr("Cannot access clipboard data!"), LogError );
        return;
    }

    // same data as last time?
    newHash = hash(*data, m_formats);
    if (m_lastHash == newHash)
        return;

    // clone only mime types defined by user
    data2 = cloneData(*data, &m_formats);
    // any data found?
    if ( data2->formats().isEmpty() ) {
        delete data2;
        return;
    }

    // send data to serve and synchronize if needed
    m_lastHash = newHash;

    clipboardChanged(mode, data2);
}

bool ClipboardMonitor::updateSelection(bool)
{
    return true;
}
#endif

void ClipboardMonitor::clipboardChanged(QClipboard::Mode, QMimeData *data)
{
    ClipboardItem item;

    item.setData(data);

    // send clipboard item
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << item;
    writeMessage(m_socket, msg);
}

void ClipboardMonitor::updateTimeout()
{
    if (m_newdata)
        updateClipboard(m_newdata, true);
}

void ClipboardMonitor::readyRead()
{
    m_socket->blockSignals(true);
    while ( m_socket->bytesAvailable() ) {
        QByteArray msg;
        if( !readMessage(m_socket, &msg) ) {
            log( tr("Cannot read message from server!"), LogError );
            return;
        }
        ClipboardItem item;
        QDataStream in(&msg, QIODevice::ReadOnly);
        in >> item;
        updateClipboard( cloneData(*item.data()) );
    }
    m_socket->blockSignals(false);
}

void ClipboardMonitor::updateClipboard(QMimeData *data, bool force)
{
    if (m_newdata && m_newdata != data)
        delete m_newdata;

    m_newdata = data;
    if ( !force && m_updatetimer.isActive() )
        return;

    m_lastHash = hash(*data, data->formats());
    setClipboardData(data, QClipboard::Clipboard);
#ifdef Q_WS_X11
    setClipboardData(cloneData(*data), QClipboard::Selection);
#endif

    m_newdata = NULL;

    m_updatetimer.start();
}

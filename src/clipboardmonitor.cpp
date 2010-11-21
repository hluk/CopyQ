#include "clipboardmonitor.h"
#include "clipboardserver.h"
#include "configurationmanager.h"
#include "clipboarditem.h"
#include <QMimeData>
#include <QDebug>
#include <QTime>
#include <QFile>

#ifndef WIN32
#include <X11/extensions/XInput.h>
#endif

ClipboardMonitor::ClipboardMonitor(int &argc, char **argv) :
    App(argc, argv) ,m_lastClipboard(0), m_lastSelection(0),
    m_newdata(NULL)
{
    m_socket = new QLocalSocket(this);
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
    connect( m_socket, SIGNAL(disconnected()),
             this, SLOT(quit()) );
    m_socket->connectToServer( ClipboardServer::monitorServerName() );
    if ( !m_socket->waitForConnected(2000) )
        return;

    ConfigurationManager *cm = ConfigurationManager::instance();
    setInterval( cm->value(ConfigurationManager::Interval).toInt() );
    setFormats( cm->value(ConfigurationManager::Formats).toString() );
    setCheckClipboard( cm->value(ConfigurationManager::CheckClipboard).toBool() );
    setCopyClipboard( cm->value(ConfigurationManager::CopyClipboard).toBool() );
    setCheckSelection( cm->value(ConfigurationManager::CheckSelection).toBool() );
    setCopySelection( cm->value(ConfigurationManager::CopySelection).toBool() );

    m_timer.setSingleShot(true);
    connect(&m_timer, SIGNAL(timeout()),
            this, SLOT(timeout()));
    m_timer.start();

    m_updatetimer.setSingleShot(true);
    m_updatetimer.setInterval(500);
    connect( &m_updatetimer, SIGNAL(timeout()), SLOT(updateTimeout()) );
}

void ClipboardMonitor::setFormats(const QString &list)
{
    m_formats = list.split( QRegExp("[;, ]+") );
}

void ClipboardMonitor::timeout()
{
    if (m_checkclip || m_checksel) {
#ifndef WIN32
        // wait while selection is incomplete, i.e. mouse button or
        // shift key is pressed
        // FIXME: in VIM to make a selection you only need to hold
        //        direction key in visual mode
        Display *dsp = XOpenDisplay( NULL );
        Window root = DefaultRootWindow(dsp);
        XEvent event;

        XQueryPointer(dsp, root,
                      &event.xbutton.root, &event.xbutton.window,
                      &event.xbutton.x_root, &event.xbutton.y_root,
                      &event.xbutton.x, &event.xbutton.y,
                      &event.xbutton.state);
        if( event.xbutton.state &
            (Button1Mask | ShiftMask) ) {
            m_timer.start();
            return;
        }

        XCloseDisplay(dsp);
#endif

        checkClipboard();
    }
    m_timer.start();
}

uint ClipboardMonitor::hash(const QMimeData &data)
{
    QByteArray bytes;
    foreach( QString mime, m_formats ) {
        bytes = data.data(mime);
        if ( !bytes.isEmpty() )
            break;
    }

    return qHash(bytes);
}

void ClipboardMonitor::checkClipboard()
{
    const QMimeData *d, *data, *data2;
    d = data = data2 = NULL;
    uint h = m_lastClipboard;
    uint h2 = m_lastSelection;

    if ( !clipboardLock.tryLock() )
        return;

    // clipboard
    QClipboard *clipboard = QApplication::clipboard();

    // clipboard data
    if ( m_checkclip ) {
        data = clipboard->mimeData(QClipboard::Clipboard);
    }
    if ( m_checksel ) { // don't handle selections in own window
        data2 = clipboard->mimeData(QClipboard::Selection);
    }

    // hash
    if (data)
        h = hash(*data);
    if (data2)
        h2 = hash(*data2);

    // check hash value
    //   - synchronize clipboards only when data are different
    QClipboard::Mode mode;
    if ( h != m_lastClipboard ) {
        d = data;
        mode = QClipboard::Clipboard;
        m_lastClipboard = h;
    } else if ( h2 != m_lastSelection && !d ) {
        d = data2;
        mode = QClipboard::Selection;
        m_lastSelection = h2;
    }

    if ( d && h != h2 ) {
        // clipboard content was changed -> synchronize
        if( mode == QClipboard::Clipboard &&
            m_copyclip &&
            h2 == m_lastSelection ) {
            clipboard->setMimeData( cloneData(*d, &m_formats),
                                    QClipboard::Selection );
        } else if ( mode == QClipboard::Selection &&
                    m_copysel &&
                    h == m_lastClipboard ) {
            clipboard->setMimeData( cloneData(*d, &m_formats),
                                    QClipboard::Clipboard );
        }

    }

    if (d) {
        // create and emit new clipboard data
        clipboardChanged(mode, cloneData(*d, &m_formats));
    }

    clipboardLock.unlock();
}

void ClipboardMonitor::clipboardChanged(QClipboard::Mode, QMimeData *data)
{
    ClipboardItem item;

    foreach ( QString mime, data->formats() ) {
        item.setData(mime, data->data(mime));
    }

    // send clipboard item
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << item;
    QByteArray zipped = qCompress(bytes);

    QDataStream(m_socket) << (quint32)zipped.length() << zipped;
    m_socket->flush();
}

void ClipboardMonitor::updateTimeout()
{
    if (m_newdata) {
        clipboardLock.lock();
        QMimeData *data = m_newdata;
        m_newdata = NULL;
        clipboardLock.unlock();

        updateClipboard(*data, true);

        delete data;
    }
}

void ClipboardMonitor::readyRead()
{
    QByteArray msg;
    if( !readBytes(m_socket, msg) ) {
        // something is wrong -> exit
        qDebug( tr("ERROR: Incorrect message received!").toLocal8Bit() );
        exit(3);
        return;
    }

    QDataStream in2(&msg, QIODevice::ReadOnly);

    ClipboardItem item;
    in2 >> item;

    updateClipboard( *item.data() );
}

void ClipboardMonitor::updateClipboard(const QMimeData &data, bool force)
{
    if ( !clipboardLock.tryLock() ) {
        return;
    }

    uint h = hash(data);
    if ( h == m_lastClipboard && h == m_lastSelection ) {
        // data already in clipboard
        clipboardLock.unlock();
        return;
    }

    if (m_newdata) {
        delete m_newdata;
    }
    m_newdata = cloneData(data);

    if ( !force && m_updatetimer.isActive() ) {
        clipboardLock.unlock();
        return;
    }

    QMimeData* newdata2 = cloneData(data);

    QClipboard *clipboard = QApplication::clipboard();
    if ( h != m_lastClipboard ) {
        clipboard->setMimeData(m_newdata, QClipboard::Clipboard);
        m_lastClipboard = h;
    }
    if ( h != m_lastSelection ) {
        clipboard->setMimeData(newdata2, QClipboard::Selection);
        m_lastSelection = h;
    }

    m_newdata = NULL;

    m_updatetimer.start();

    clipboardLock.unlock();
}

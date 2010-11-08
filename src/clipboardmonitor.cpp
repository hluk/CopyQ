#include "clipboardmonitor.h"
#include "client_server.h"
#include "configurationmanager.h"
#include <qtlocalpeer.h>
#include <QMimeData>
#include <QApplication>
#include <QDebug>
#include <QTime>

#ifndef WIN32
#include <X11/extensions/XInput.h>
#endif

ClipboardMonitor::ClipboardMonitor(QWidget *parent) :
    QObject(parent), m_lastSelection(0),
    m_newdata(NULL)
{
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
        // wait on selection completed
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
    uint h, h2;
    h = h2 = m_lastSelection;

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
    if ( h != m_lastSelection ) {
        d = data;
        mode = QClipboard::Clipboard;
        m_lastSelection = h;
    } else if ( h2 != m_lastSelection && !d ) {
        d = data2;
        mode = QClipboard::Selection;
        m_lastSelection = h2;
    }

    if ( d && h != h2 ) {
        // clipboard content was changed -> synchronize
        if( mode == QClipboard::Clipboard && m_copyclip ) {
            clipboard->setMimeData( cloneData(*d, true),
                                    QClipboard::Selection );
        } else if ( mode == QClipboard::Selection && m_copysel ) {
            clipboard->setMimeData( cloneData(*d, true),
                                    QClipboard::Clipboard );
        }

    }

    if (d) {
        // create and emit new clipboard data
        clipboardChanged(mode, cloneData(*d, true));
    }

    clipboardLock.unlock();
}

void ClipboardMonitor::clipboardChanged(QClipboard::Mode mode, QMimeData *data)
{
    QString msg;
    DataList args;

    args << QByteArray("_write");
    foreach ( QString mime, data->formats() ) {
        args << mime.toLocal8Bit() << data->data(mime);
    }
    serialize_args(args, msg);

    QtLocalPeer peer( NULL, QString("CopyQ") );
    // is server running?
    if ( peer.isClient() ) {
        peer.sendMessage(msg, 2000);
    }
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

void ClipboardMonitor::handleMessage(const QString &message)
{
    DataList args;
    deserialize_args(message, args);

    const QString cmd = args.isEmpty() ? QString() : args.takeFirst();

    if ( cmd == "write" ) {
        QString mime;

        QMimeData data;
        while ( !args.isEmpty() ) {
            // get mime type
            if ( !parse(args, &mime) )
                break;

            // get data
            if ( args.isEmpty() ) {
                break;
            } else {
                data.setData( mime, args.takeFirst() );
            }
        }
        updateClipboard(data);
    }
}

void ClipboardMonitor::updateClipboard(const QMimeData &data, bool force)
{
    if ( !clipboardLock.tryLock() ) {
        return;
    }

    uint h = hash(data);
    if ( h == m_lastSelection ) {
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
    clipboard->setMimeData(m_newdata, QClipboard::Clipboard);
    clipboard->setMimeData(newdata2, QClipboard::Selection);
    m_lastSelection = h;

    m_newdata = NULL;

    m_updatetimer.start();

    clipboardLock.unlock();
}

QMimeData *ClipboardMonitor::cloneData(const QMimeData &data, bool filter)
{
    QMimeData *newdata = new QMimeData;
    if (filter) {
        foreach( QString mime, m_formats ) {
            QByteArray bytes = data.data(mime);
            if ( !bytes.isEmpty() )
                newdata->setData(mime, bytes);
        }
    } else {
        foreach( QString mime, data.formats() ) {
            newdata->setData(mime, data.data(mime));
        }
    }
    return newdata;
}

#include "clipboardmonitor.h"
#include "clipboardserver.h"
#include "configurationmanager.h"
#include "clipboarditem.h"
#include <QMimeData>

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

ClipboardMonitor::ClipboardMonitor(int &argc, char **argv) :
    App(argc, argv), m_newdata(NULL), m_lastClipboard(0)
#ifdef Q_WS_X11
  , m_lastSelection(0), m_dsp(NULL)
#endif
{
    m_socket = new QLocalSocket(this);
    connect( m_socket, SIGNAL(readyRead()),
             this, SLOT(readyRead()) );
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

#ifdef Q_WS_X11
    setCopyClipboard( cm->value("copy_clipboard").toBool() );
    setCheckSelection( cm->value("check_selection").toBool() );
    setCopySelection( cm->value("copy_selection").toBool() );
    m_timer.setSingleShot(true);
    m_timer.setInterval(100);
    connect(&m_timer, SIGNAL(timeout()),
            this, SLOT(updateSelection()));
#endif

    m_updatetimer.setSingleShot(true);
    m_updatetimer.setInterval(500);
    connect( &m_updatetimer, SIGNAL(timeout()),
             this, SLOT(updateTimeout()) );

    connect( QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)),
             this, SLOT(checkClipboard(QClipboard::Mode)) );

#ifdef Q_WS_X11
    checkClipboard(QClipboard::Selection);
#endif
    checkClipboard(QClipboard::Clipboard);
}

ClipboardMonitor::~ClipboardMonitor()
{
#ifdef Q_WS_X11
    if (m_dsp)
        XCloseDisplay(m_dsp);
#endif
}

void ClipboardMonitor::setFormats(const QString &list)
{
    m_formats = list.split( QRegExp("[;, ]+") );
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

#ifdef Q_WS_X11
bool ClipboardMonitor::updateSelection(bool check_clipboard)
{
    // wait while selection is incomplete, i.e. mouse button or
    // shift key is pressed
    if ( m_timer.isActive() )
        return false;
    if ( m_dsp || (m_dsp = XOpenDisplay(NULL)) ) {
        Window root = DefaultRootWindow(m_dsp);
        XEvent event;

        XQueryPointer(m_dsp, root,
                      &event.xbutton.root, &event.xbutton.window,
                      &event.xbutton.x_root, &event.xbutton.y_root,
                      &event.xbutton.x, &event.xbutton.y,
                      &event.xbutton.state);

        if( event.xbutton.state &
                (Button1Mask | ShiftMask) ) {
            m_timer.start();
            return false;
        }
    }

    if (check_clipboard) {
        checkClipboard(QClipboard::Selection);
    }
    return true;
}

void ClipboardMonitor::checkClipboard(QClipboard::Mode mode)
{
    QClipboard *clipboard;
    const QMimeData *data;
    uint new_hash;
    uint *hash1 = NULL, *hash2 = NULL;
    bool synchronize;
    QClipboard::Mode mode2;

    if ( m_checkclip && mode == QClipboard::Clipboard ) {
        mode2 = QClipboard::Selection;
        hash1 = &m_lastClipboard;
        hash2 = &m_lastSelection;
        synchronize = m_copyclip;
    } else if ( m_checksel && mode == QClipboard::Selection &&
                updateSelection(false) ) {
        mode2 = QClipboard::Clipboard;
        hash1 = &m_lastSelection;
        hash2 = &m_lastClipboard;
        synchronize = m_copysel;
    } else {
        return;
    }

    clipboard = QApplication::clipboard();
    data = clipboard->mimeData(mode);
    if (data) {
        new_hash = hash(*data);
        // is clipboard data different?
        if ( new_hash != *hash1 ) {
            *hash1 = new_hash;
            /* do something only if data are not empty and
               if text is present it is not empty */
            if ( !data->formats().isEmpty() &&
                 (!data->hasText() || data->text().size() > 0) ) {
                // synchronize clipboard and selection
                if( synchronize && new_hash != *hash2 ) {
                    *hash2 = new_hash;
                    clipboard->setMimeData( cloneData(*data, &m_formats), mode2 );
                }
                clipboardChanged(mode, cloneData(*data, &m_formats));
            }
        }
    }
}
#else /* !Q_WS_X11 */
void ClipboardMonitor::checkClipboard(QClipboard::Mode mode)
{
    if ( m_checkclip && mode == QClipboard::Clipboard ) {
        const QMimeData *data;
        QClipboard *clipboard = QApplication::clipboard();
        data = clipboard->mimeData(mode);
        /* do something only if data are not empty and
           if text is present it is not empty */
        if ( data && !data->formats().isEmpty() &&
             (!data->hasText() || data->text().size() > 0) ) {
            clipboardChanged(mode, cloneData(*data, &m_formats));
        }
    }
}

bool ClipboardMonitor::updateSelection(bool check_clipboard)
{
    return true;
}
#endif

void ClipboardMonitor::clipboardChanged(QClipboard::Mode, QMimeData *data)
{
    ClipboardItem item;

    foreach ( QString mime, data->formats() ) {
        item.setData(mime, data->data(mime));
    }

    // send clipboard item
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << item;
    writeMessage(m_socket, msg);
}

void ClipboardMonitor::updateTimeout()
{
    if (m_newdata) {
        QMimeData *data = m_newdata;
        m_newdata = NULL;

        updateClipboard(*data, true);

        delete data;
    }
}

void ClipboardMonitor::readyRead()
{
    QByteArray msg;
    if( !readMessage(m_socket, &msg) ) {
        log( tr("Cannot read message from server!"), LogError );
        return;
    }
    ClipboardItem item;
    QDataStream in(&msg, QIODevice::ReadOnly);
    in >> item;
    updateClipboard( *(item.data()) );
}

void ClipboardMonitor::updateClipboard(const QMimeData &data, bool force)
{
    uint h = hash(data);
    if ( h == m_lastClipboard
#ifdef Q_WS_X11
            && h == m_lastSelection
#endif
            ) {
        // data already in clipboard
        return;
    }

    if (m_newdata) {
        delete m_newdata;
    }
    m_newdata = cloneData(data);

    if ( !force && m_updatetimer.isActive() ) {
        return;
    }

#ifdef Q_WS_X11
    QMimeData* newdata2 = cloneData(data);
#endif

    QClipboard *clipboard = QApplication::clipboard();
    if ( h != m_lastClipboard ) {
        m_lastClipboard = h;
        clipboard->setMimeData(m_newdata, QClipboard::Clipboard);
    } else {
        delete m_newdata;
    }
#ifdef Q_WS_X11
    if ( h != m_lastSelection ) {
        m_lastSelection = h;
        clipboard->setMimeData(newdata2, QClipboard::Selection);
    } else {
        delete newdata2;
    }
#endif

    m_newdata = NULL;

    m_updatetimer.start();
}

#include "clipboardmonitor.h"
#include <QMimeData>
#include <QApplication>
#include <QDebug>

#ifndef WIN32
#include <X11/extensions/XInput.h>
#endif

ClipboardMonitor::ClipboardMonitor(QWidget *parent) :
    QObject(parent), m_interval(1000), m_lastSelection(0),
    m_checkclip(true), m_checksel(true),
    m_copyclip(true), m_copysel(true)
{
    m_parent = parent;
    m_timer.setInterval(1000);
    m_timer.setSingleShot(true);
    connect(&m_timer, SIGNAL(timeout()),
            this, SLOT(timeout()));

    m_formats << QString("text/plain")
              << QString("image/bmp")
              << QString("image/x-inkscape-svg-compressed")
              << QString("text/html");
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
    if ( m_checksel && !m_parent->hasFocus() ) { // don't handle selections in own window
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

    clipboardLock.unlock();

    if (d) {
        // create and emit new clipboard data
        emit clipboardChanged(mode, cloneData(*d));
    }
}

void ClipboardMonitor::updateClipboard(QClipboard::Mode mode, const QMimeData &data)
{
    if ( !clipboardLock.tryLock() )
        return;

    // clone mime data before calling processEvents and
    // eventually changing the memory alocated for data variable
    QMimeData *newdata = cloneData(data);
    //qApp->processEvents();

    QClipboard *clipboard = QApplication::clipboard();

    m_lastSelection = hash(*newdata);
    clipboard->setMimeData(newdata, mode);

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

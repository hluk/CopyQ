#include "clipboardmonitor.h"
#include <QMimeData>
#include <QApplication>
#include <QDebug>

#ifndef WIN32
#include <X11/extensions/XInput.h>
#endif

ClipboardMonitor::ClipboardMonitor(QWidget *parent) :
    QObject(parent), m_interval(1000), m_re_format("^[a-z]"), m_lastSelection(0)
{
    m_parent = parent;
    m_timer.setInterval(1000);
    m_timer.setSingleShot(true);
    connect(&m_timer, SIGNAL(timeout()),
            this, SLOT(timeout()));
}

void ClipboardMonitor::timeout()
{
    checkClipboard(QClipboard::Clipboard) ||
            checkClipboard(QClipboard::Selection);
    m_timer.start();
}

uint ClipboardMonitor::hash(const QMimeData &data)
{
    const QStringList formats = data.formats();

    if (formats.isEmpty())
        return 0;

    QByteArray bytes = data.data( formats.first() );

    if (bytes.isEmpty())
        return 0;

    return qHash(bytes);
/*
    int l = bytes.length();
    uint h = l;
    h ^= (uint(bytes.at(0))<<16);
    h ^= (uint(bytes.at(l))<<24);

    return h;
*/
}

bool ClipboardMonitor::checkClipboard(QClipboard::Mode mode)
{
    // wait on selection completed
#ifndef WIN32
    if ( mode == QClipboard::Selection )
    {

        // don't handle selections in own window
        if (m_parent->hasFocus()) {
            return false;
        }
        Display *dsp = XOpenDisplay( NULL );
        Window root = DefaultRootWindow(dsp);
        XEvent event;

        // wait while mouse button, shift or ctrl keys are pressed
        // (i.e. selection is incomplete)
        while(true) {
            XQueryPointer(dsp, root,
                          &event.xbutton.root, &event.xbutton.window,
                          &event.xbutton.x_root, &event.xbutton.y_root,
                          &event.xbutton.x, &event.xbutton.y,
                          &event.xbutton.state);
            if( !(event.xbutton.state &
                  (Button1Mask | ShiftMask | ControlMask)) )
                break;
            usleep(500);
        };

        XCloseDisplay(dsp);
    }
#endif

    clipboardLock.lock();

    bool result = false;
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *data = clipboard->mimeData(mode);

    if (data) {
        // does clipboard data differ?
        uint h = hash(*data);
        if ( h && h != m_lastSelection ) {
            m_lastSelection = h;

            //synchronize clipboard and selection
            QClipboard::Mode mode2 = mode == QClipboard::Clipboard ?
                                     QClipboard::Selection : QClipboard::Clipboard;
            clipboard->setMimeData(cloneData(*data), mode2);

            result = true;
        }
    }

    clipboardLock.unlock();

    if (result) {
        // create and emit new clipboard data
        emit clipboardChanged(mode, cloneData(*data, true));
    }

    return result;
}

void ClipboardMonitor::updateClipboard(QClipboard::Mode mode, const QMimeData &data)
{
    clipboardLock.lock();

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(data), mode);

    m_lastSelection = hash(data);

    clipboardLock.unlock();
}

QMimeData *ClipboardMonitor::cloneData(const QMimeData &data, bool filter)
{
    QMimeData *newdata = new QMimeData;
    foreach( QString mime, data.formats() ) {
        if ( !filter || m_re_format.indexIn(mime) != -1 )
            newdata->setData(mime, data.data(mime));
    }

    return newdata;
}

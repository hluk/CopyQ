/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

#include "clipboardbrowser.h"
#include <QDebug>
#include <QKeyEvent>
#include <QApplication>
#include <X11/extensions/XInput.h>
#include <unistd.h> //usleep
#include <actiondialog.h>
#include "itemdelegate.h"
#include "clipboardmodel.h"

inline bool readDatFile(QIODevice &device, QSettings::SettingsMap &map)
{
    QDataStream in(&device);
    in >> map["items"];
    return true;
}

inline bool writeDatFile(QIODevice &device, const QSettings::SettingsMap &map)
{
    QDataStream out(&device);
    out << map["items"];
    return true;
}

extern int error_handler(Display *dsp, XErrorEvent *err)
{
    char buff[256];
    XGetErrorText(dsp, err->error_code, buff, 256);
    qDebug() << buff;

    return 0;
}

ClipboardBrowser::ClipboardBrowser(QWidget *parent) : QListView(parent),
    datFormat( QSettings::registerFormat( QString("dat"), readDatFile, writeDatFile) ),
    datSettings( datFormat, QSettings::UserScope,
            QCoreApplication::organizationName(),
            QCoreApplication::applicationName() )
{
    m_clip = QApplication::clipboard();
    actionDialog = NULL;

    // delegate for rendering and editing items
    d = new ItemDelegate(this);
    setItemDelegate(d);

    // set new model
    m = new ClipboardModel();
    QItemSelectionModel *old_model = selectionModel();
    setModel(m);
    delete old_model;

    connect( this, SIGNAL(doubleClicked(QModelIndex)),
            SLOT(moveToClipboard(QModelIndex)));

    // ScrollPerItem doesn't work well with hidden items
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

void ClipboardBrowser::startMonitoring()
{
    connect( m_clip, SIGNAL(changed(QClipboard::Mode)),
            this, SLOT(clipboardChanged(QClipboard::Mode)) );

    // check clipboard in intervals
    timer.start(1000, this);
}

void ClipboardBrowser::stopMonitoring()
{
    disconnect( m_clip, SIGNAL(changed(QClipboard::Mode)),
            this, SLOT(clipboardChanged(QClipboard::Mode)) );
    timer.stop();
}

ClipboardBrowser::~ClipboardBrowser()
{
    timer.stop();
    emit closeAllEditors();
}

void ClipboardBrowser::closeEditor(QEditor *editor)
{
    // check if file was modified before closing
    if ( editor->fileModified() )
        itemModified( editor->getHash(), editor->getText() );

    editor->disconnect(this);
    disconnect(editor);
    delete editor;
}

void ClipboardBrowser::openEditor()
{
    QEditor *editor = new QEditor(itemText(), m_editor);

    connect( editor, SIGNAL(fileModified(uint, const QString &)),
            this, SLOT(itemModified(uint, const QString &)) );

    connect( editor, SIGNAL(closed(QEditor *)),
            this, SLOT(closeEditor(QEditor *)) );

    connect( this, SIGNAL(closeAllEditors()), editor, SLOT(close()) );

    if ( !editor->start() )
        closeEditor(editor);
}

void ClipboardBrowser::itemModified(uint hash, const QString &str)
{
    // find item with qHash == hash and remove it
    for(int i = 0; i < m->rowCount(); ++i) {
        if ( qHash(itemText(i)) == hash ) {
            m->removeRow(i);
            break;
        }
    }

    // add new item
    add(str);
    sync();
}

void ClipboardBrowser::filterItems(const QString &str)
{
    // if search string empty: all items visible
    if ( str.isEmpty() ) {
        m->setSearch();
        emit hideSearch();
    }
    else {
        // if search string is a number N: highlight Nth item
        bool ok;
        int n = str.toInt(&ok);
        if (ok) {
            m->setSearch();
            setCurrent(n);
            return;
        }
        else {
            QRegExp re(str);
            re.setCaseSensitivity(Qt::CaseInsensitive);
            m->setSearch(&re);
        }
    }

    // hide filtered items
    reset();
    int first = -1;
    for(int i = 0; i < m->rowCount(); ++i) {
        if ( m->isFiltered(i) )
            setRowHidden(i,true);
        else if (first == -1)
                first = i;
    }
    // select first visible
    setCurrentIndex( index(first) );
}

void ClipboardBrowser::moveToClipboard(const QModelIndex &ind)
{
    if ( ind.isValid() )
        moveToClipboard(ind.row());
}

void ClipboardBrowser::moveToClipboard(const QString &txt)
{
    // TODO: remove identical; How to iterate through all items?
    // insert new
    if ( m->insertRow(0) ) {
        m->setData( index(0), txt );
        sync();
    }
}

void ClipboardBrowser::moveToClipboard(int i)
{
    if ( i > 0 ) {
        m->move(i,0);
        sync();
        repaint();
    }
}

void ClipboardBrowser::timerEvent(QTimerEvent *event)
{
    if ( event->timerId() == timer.timerId() ) {
        if ( m_clip->text(QClipboard::Selection) != itemText(0) )
            add(m_clip->text(QClipboard::Selection));
    }
    else if ( event->timerId() == timer_save.timerId() ) {
        saveItems();
        timer_save.stop();
    }
    else
        QListView::timerEvent(event);
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    QModelIndex ind;

    if ( state() == QAbstractItemView::EditingState ) {
        QListView::keyPressEvent(event);
        return;
    }

    // CTRL
    if ( event->modifiers() == Qt::ControlModifier ) {
        switch ( event->key() ) {
        // CTRL-E: open external editor
        case Qt::Key_E:
            openEditor();
            break;
        // CTRL-N: create new item
        case Qt::Key_N:
            add("<NEW>", false);
            setCurrent(0);
            edit( index(0) );
            break;
        // CTRL-A: action
        case Qt::Key_A:
            if (!actionDialog) {
                actionDialog = new ActionDialog(this);
                connect( actionDialog, SIGNAL(addItems(const QStringList)),
                         this, SLOT(addItems(const QStringList&)) );
                connect( actionDialog, SIGNAL(error(const QString)),
                         this, SIGNAL(error(const QString)) );
            }
            actionDialog->setInput( itemText() );
            actionDialog->exec();
            break;
        // CTRL-Up/Down: move item
        case Qt::Key_Up:
        case Qt::Key_Down:
            int from = currentIndex().row();
            int to = (event->key() == Qt::Key_Up) ? (from-1) : (from+1);
            if ( m->move(from, to) ) {
                if (from == 0 || to == 0 || to == m->rowCount())
                    sync();
                scrollTo( currentIndex() );
                repaint();
            }
            break;
        }
    }
    else {
        switch ( event->key() ) {
        case Qt::Key_Delete:
            remove();
            break;

        // navigation
        case Qt::Key_Up:
            if ( selectedIndexes().isEmpty() )
                setCurrent(-1, true);
            else
                setCurrent( currentIndex().row()-1, true );
            break;
        case Qt::Key_Down:
            if ( selectedIndexes().isEmpty() )
                setCurrent(0);
            else
                setCurrent( currentIndex().row()+1, true );
            break;
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_Escape:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            QListView::keyPressEvent(event);
            break;

        case Qt::Key_F2:
            QListView::keyPressEvent(event);
            break;

        case Qt::Key_Enter:
        case Qt::Key_Return:
            moveToClipboard( currentIndex().row() );
            break;

        default:
                emit requestSearch(event);
                //QListWidget::keyPressEvent(event);
            break;
        }
    }
}

void ClipboardBrowser::dataChanged(const QModelIndex &first, const QModelIndex &last)
{
    for( int i = first.row(); i<=last.row(); ++i)
        on_itemChanged(i);
    QListView::dataChanged(first, last);
}

void ClipboardBrowser::on_itemChanged( int i )
{
    // remove empty item
    if ( itemText(i).indexOf( QRegExp("^\\s*$") ) != -1 )
        m->removeRow(i);
    // first item (clipboard contents) changed
    else if ( i == 0 )
        sync();
}

void ClipboardBrowser::setCurrent(int row, bool cycle)
{
    // direction
    int cur = currentIndex().row();
    int dir = cur < row ? 1 : -1;

    // select first visible
    int i = m->getRowNumber(row, cycle);
    cur = i;
    while ( isRowHidden(i) ) {
        i = m->getRowNumber(i+dir, cycle);
        if ( (!cycle && (i==0 || i==m->rowCount()-1)) || i == cur)
            break;
    }

    QModelIndex ind = index(i);
    setCurrentIndex(ind);
}

void ClipboardBrowser::remove()
{
    QItemSelectionModel *sel = selectionModel();

    QModelIndexList list = sel->selectedIndexes();
    int i = 0;
    int n = 0;
    foreach(QModelIndex ind, list) {
        i = ind.row();
        m->removeRow(i-n);
        ++n;
        if ( i == 0 )
            sync();
        if ( !timer_save.isActive() )
            timer_save.start(30000, this);
    }
    // select next
    setCurrent(i-n+1);
}

bool ClipboardBrowser::add(const QString &txt, bool ignore_empty)
{
    // ignore empty
    if ( ignore_empty && txt.indexOf( QRegExp("^\\s*$") ) != -1 )
        return false;

    // find same item
    int i = 0;
    for(; i < m->rowCount(); i++) {
        if( txt == itemText(i) )
           break;
    }
    // move found same item to top
    if ( i < m->rowCount() ) {
        moveToClipboard(i);
    }
    else {
        // create new item
        m->insertRow(0);
        QModelIndex ind = index(0);
        m->setData(ind, txt);

        // filter item
        if ( m->isFiltered(0) )
            setRowHidden(0,true);

        // list size limit
        if ( m->rowCount() > m_maxitems )
            m->removeRow( m->rowCount() - 1 );

        // save
        if ( !timer_save.isActive() )
            timer_save.start(30000, this);
    }
    
    return true;
}

void ClipboardBrowser::addItems(const QStringList &items) {
    QStringList::const_iterator it( items.end()-1 );
    for(; it != items.begin()-1; --it)
        add(*it);
}

void ClipboardBrowser::readSettings(const QString &css)
{
    QSettings settings;

    setStyleSheet(css);
    d->setStyleSheet(css);

    // restore configuration
    m_maxitems = settings.value("maxitems", 400).toInt();
    m_editor = settings.value("editor", "gvim -f %1").toString();
    d->setItemFormat( settings.value("format",
        "<div class=\"item\"><div class=\"number\">%1</div><div class=\"text\">%2</div></div>"
        ).toString()
    );

    // restore items
    QSettings::Format datFormat(
            QSettings::registerFormat( QString("dat"), readDatFile, writeDatFile) );
    QSettings datSettings( datFormat, QSettings::UserScope,
            QCoreApplication::organizationName(),
            QCoreApplication::applicationName() );
    QStringList items = datSettings.value("items", QStringList()).toStringList();

    // set new model
    m = new ClipboardModel(items);
    QItemSelectionModel *old_model = selectionModel();
    setModel(m);
    delete old_model;

    m->setMaxItems( settings.value("maxitems", 400).toInt() );
    // TODO: set external editor
    //m->setEditor( settings.value("editor", "gvim -f %1").toString() );

    timer_save.stop();

    sync(false);
}

void ClipboardBrowser::writeSettings()
{
    QSettings settings;

    settings.setValue( "maxitems", m_maxitems );
    settings.setValue( "editor", m_editor );
    settings.setValue( "format", d->itemFormat() );
}

void ClipboardBrowser::saveItems()
{
    QStringList m_lst;
    for(int i = 0; i < m->rowCount(); ++i)
        m_lst.append( itemText(i) );

    datSettings.setValue("items", m_lst);
}

QString ClipboardBrowser::itemText(int i) const
{
    if ( i >= m->rowCount() )
        return QString();
    return itemText( (i==-1) ? currentIndex() : index(i) );
}

QString ClipboardBrowser::itemText(QModelIndex ind) const
{
    return ind.isValid() ? ind.data(Qt::EditRole).toString() : QString();
}

void ClipboardBrowser::clipboardChanged(QClipboard::Mode mode)
{
    if ( mode == QClipboard::Selection )
    {
        timer.stop();

        // don't handle selections
        // already handled or
        // made in this browser
        if ( hasFocus() ) return;

        // handle selection only if mouse button not held
        XSetErrorHandler(error_handler);
        Display *dsp = XOpenDisplay( NULL );
        Window root = DefaultRootWindow(dsp);
        XEvent event;

        // active wait on mouse button release
        do {
            XQueryPointer(dsp, root,
                    &event.xbutton.root, &event.xbutton.window,
                    &event.xbutton.x_root, &event.xbutton.y_root,
                    &event.xbutton.x, &event.xbutton.y,
                    &event.xbutton.state);
            usleep(200);
        } while (event.xbutton.state & Button1Mask);
        
        XCloseDisplay(dsp);

        timer.start(1000, this);
    }

    sync(false);
}

void ClipboardBrowser::sync(bool list_to_clipboard)
{
    QString text;

    // first item -> clipboard
    if (list_to_clipboard) {
        if ( m->rowCount() == 0 )
            return;
        text = itemText(0);
    }
    // clipboard -> first item
    else {
        text = m_clip->text();
        if( text != itemText(0) ) {
            if ( !add(text) )
                return;
        }
        else {
            text = m_clip->text(QClipboard::Selection);
            if( text != itemText(0) ) {
                if ( !add(text) )
                    return;
            }
            else
                return;
        }
    }

    // sync Clipboard == Selection
    stopMonitoring();
    if ( m_clip->text(QClipboard::Selection) != text )
        m_clip->setText(text,QClipboard::Selection);
    if ( m_clip->text() != text )
        m_clip->setText(text);
    startMonitoring();
}

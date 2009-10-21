/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

    This file is part of Copyq.

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
#include <QFile>
#include <QApplication>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QScrollBar>
#include <X11/extensions/XInput.h>
#include <unistd.h> //usleep
#include "itemdelegate.h"

extern int error_handler(Display *dsp, XErrorEvent *err)
{
    char buff[256];
    XGetErrorText(dsp, err->error_code, buff, 256);
    qDebug() << buff;

    return 0;
}

ClipboardBrowser::ClipboardBrowser(QWidget *parent) : QListWidget(parent)
{
    setItemDelegate( new ItemDelegate(this) );

    m_ctrlmod = m_shiftmod = false;
    m_clip = QApplication::clipboard();

    connect( m_clip, SIGNAL(changed(QClipboard::Mode)),
            this, SLOT(clipboardChanged(QClipboard::Mode)) );
    connect( this, SIGNAL(itemChanged(QListWidgetItem *)),
            SLOT(on_itemChanged(QListWidgetItem *)));
    connect( this, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
            SLOT(moveToClipboard(QListWidgetItem *)));

    // settings
    readSettings();

    // check clipboard in intervals
    timer.start(1000, this);
}

ClipboardBrowser::~ClipboardBrowser()
{
    timer.stop();
    emit closeAllEditors();
}

void ClipboardBrowser::closeEditor(QEditor *editor)
{
    editor->disconnect(this);
    disconnect(editor);
    delete editor;
}

void ClipboardBrowser::openEditor()
{
    QEditor *editor = new QEditor(currentItem()->text(), m_editor);

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
    for(int i = 0; i < count(); ++i) {
        QListWidgetItem *it = item(i);
        if ( qHash(it->text()) == hash ) {
            delete takeItem(i);
            break;
        }
    }

    // add new item
    add(str);
}

void ClipboardBrowser::filterItems(const QString &str)
{
    // if serch string empty: all items visible
    if ( str.isEmpty() ) {
        m_search = QRegExp();
        for(int i = 0; i < count(); ++i)
            item(i)->setHidden(false);
    }
    else {
        m_search = QRegExp(str);
        m_search.setCaseSensitivity( Qt::CaseInsensitive );
        QListWidgetItem *current = NULL;

        for(int i = 0; i < count(); ++i) {
            QListWidgetItem *it = item(i);
            if ( m_search.indexIn(it->text()) == -1 )
                it->setHidden(true);
            else {
                it->setHidden(false);
                // select first visible
                if ( current == NULL )
                    current = it;
            }
        }

        setCurrentItem( current );
    }

    ((ItemDelegate *)itemDelegate())->setSearch(m_search);
}

void ClipboardBrowser::moveToClipboard(const QString &txt)
{
    // remove identical
    QList<QListWidgetItem *> list( findItems(txt, Qt::MatchExactly) );
    QList<QListWidgetItem *>::iterator it( list.begin() );
    for(; it != list.end(); ++it)
        delete takeItem( row(*it) );
    
    QListWidgetItem *x = new QListWidgetItem(txt, this);
    moveToClipboard(x);
}

void ClipboardBrowser::moveToClipboard(QListWidgetItem *x)
{
    insertItem( 0, takeItem(row(x)) );

    m_clip->disconnect(this);

    // sync clipboards
    if ( m_clip->text(QClipboard::Selection) != x->text() )
        m_clip->setText(x->text(),QClipboard::Selection);
    if ( m_clip->text() != x->text() )
        m_clip->setText(x->text());
}

void ClipboardBrowser::timerEvent(QTimerEvent *event)
{
    if ( event->timerId() == timer.timerId() ) {
        QString text;

        text = m_clip->text(QClipboard::Clipboard);
        if( !isCurrent(text) )
            add(text);
        else {
            text = m_clip->text(QClipboard::Selection);
            if( !isCurrent(text) )
                add(text);
        }
    }
    else
        QListWidget::timerEvent(event);
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    if ( state() == QAbstractItemView::EditingState ) {
        QListWidget::keyPressEvent(event);
        return;
    }

    if (m_ctrlmod) {
        if ( event->key() == Qt::Key_E )
            openEditor();
    }

    switch (event->key()) {
        case Qt::Key_Delete:
            remove();
            break;

        case Qt::Key_Control:
            m_ctrlmod = true;
            break;

        case Qt::Key_Shift:
            m_shiftmod = true;
            break;

        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_Escape:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            QListWidget::keyPressEvent(event);
            break;

        case Qt::Key_F2:
            QListWidget::keyPressEvent(event);
            break;
        
        case Qt::Key_Return:
            moveToClipboard( currentItem() );
            break;

        default:
            if ( event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier )
                emit requestSearch(event);
            else
                QListWidget::keyPressEvent(event);
            break;
    }
}

void ClipboardBrowser::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
        m_ctrlmod = false;
    if (event->key() == Qt::Key_Shift)
        m_shiftmod = false;
}

void ClipboardBrowser::on_itemChanged( QListWidgetItem *it )
{
    // first item (clipboard contents) changed
    if ( row(it) == 0 )
        moveToClipboard(it);
}

bool ClipboardBrowser::remove()
{
    QList<QListWidgetItem *> list = selectedItems();
    QList<QListWidgetItem *>::iterator it( list.begin() );
    int i = 0;
    for(; it != list.end(); ++it) {
        i = row(*it);
        delete takeItem(i);
        if ( i == 0 && count() > 0 )
            moveToClipboard(item(0));
    }
    // select next
    setCurrentRow(i);

    return list.length() > 0;
}

bool ClipboardBrowser::add(const QString &txt)
{
    // ignore empty
    if ( txt.indexOf( QRegExp("^\\s*$") ) != -1 ) {
        if ( count() > 0 )
            moveToClipboard(item(0));
        return false;
    }
    else if ( count() == 0 || !isCurrent(txt) ) {
        QListWidgetItem *x = new QListWidgetItem(txt, this);
        x->setFlags( x->flags() | Qt::ItemIsEditable );
        // filter item
        if ( !m_search.isEmpty() && m_search.indexIn(x->text()) != -1 )
            x->setHidden(true);
        moveToClipboard(x);

        if ( count() > m_maxitems )
            delete takeItem( count() - 1 );
    }
    
    return true;
}

void ClipboardBrowser::readSettings()
{
    QSettings settings;
    m_maxitems = settings.value("maxitems", 400).toInt();
    m_editor = settings.value("editor", "gvim -f %1").toString();
    QStringList m_lst = settings.value("items", QStringList()).toStringList();

    clear();
    QStringList::iterator it( m_lst.begin() );
    for(; it != m_lst.end(); ++it) {
        QListWidgetItem *x = new QListWidgetItem(*it, this);
        x->setFlags( x->flags() | Qt::ItemIsEditable );
        if ( count() > m_maxitems )
            break;
    }

    if (count() > 0)
        moveToClipboard( item(0) );
}

void ClipboardBrowser::writeSettings()
{
    QSettings settings;
    
    settings.setValue("maxitems", m_maxitems);
    settings.setValue("editor", m_editor);

    QStringList m_lst;
    for(int i = 0; i < count(); ++i)
        m_lst.append( item(i)->text() );

    settings.setValue("items", m_lst);
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

    QString txt = m_clip->text(mode);

    if ( !isCurrent(txt) )
        add(txt);
}

bool ClipboardBrowser::isCurrent(const QString &txt)
{
    QListWidgetItem *x = item(0);
    return x && x->text() == txt;
}

// vim: tags +=~/.vim/tags/qt4

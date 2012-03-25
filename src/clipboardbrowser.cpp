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
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <unistd.h> //usleep
#include <actiondialog.h>
#include "itemdelegate.h"
#include "clipboardmodel.h"
#include "clipboarditem.h"
#include "configurationmanager.h"
#include "client_server.h"

static const int max_preload = 10;

ClipboardBrowser::ClipboardBrowser(const QString &id, QWidget *parent) :
    QListView(parent), m_id(id), m_update(false), m_sizeHintChanged(false),
    m_menu(NULL)
{
    setLayoutMode(QListView::Batched);
    setBatchSize(max_preload);
    setFrameShadow(QFrame::Sunken);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTabKeyNavigation(false);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setWrapping(false);
    setLayoutMode(QListView::SinglePass);
    setEditTriggers(QAbstractItemView::EditKeyPressed);
    setSpacing(5);

    timer_save.setSingleShot(true);
    connect( &timer_save, SIGNAL(timeout()),
             this, SLOT(saveItems()) );

    // delegate for rendering and editing items
    d = new ItemDelegate(this);
    setItemDelegate(d);
    connect( d, SIGNAL(sizeHintChanged(QModelIndex)),
             this, SLOT(sizeHintChanged()), Qt::DirectConnection );

    // set new model
    m = new ClipboardModel();
    QItemSelectionModel *old_model = selectionModel();
    setModel(m);
    delete old_model;

    // context menu
    createContextMenu();

    connect( m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             d, SLOT(rowsRemoved(QModelIndex,int,int)) );
    connect( m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             d, SLOT(rowsInserted(QModelIndex, int, int)) );
    connect( m, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
             d, SLOT(rowsMoved(QModelIndex, int, int, QModelIndex, int)) );
    connect( m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             d, SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect( m, SIGNAL(realDataChanged(QModelIndex,QModelIndex)),
             this, SLOT(realDataChanged(QModelIndex,QModelIndex)));

    connect( this, SIGNAL(doubleClicked(QModelIndex)),
            SLOT(moveToClipboard(QModelIndex)));

    // ScrollPerItem doesn't work well with hidden items
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    setAttribute(Qt::WA_MacShowFocusRect, 0);

    // object name is used in CSS (i.e. #history)
    setObjectName("history");
}

ClipboardBrowser::~ClipboardBrowser()
{
    emit closeAllEditors();
    if ( timer_save.isActive() )
        saveItems();
}


void ClipboardBrowser::closeEditor(QEditor *editor)
{
    // check if file was modified before closing
    if ( editor->fileModified() )
        itemModified( editor->getText() );

    editor->disconnect(this);
    disconnect(editor);
    delete editor;
}

void ClipboardBrowser::contextMenuAction(QAction *act)
{
    QModelIndex ind;
    QVariant data = act->data();

    if ( data.isValid() ) {
        int action = data.toInt();
        switch(action) {
        case ActionToClipboard:
            moveToClipboard( currentIndex() );
            setCurrent(0);
            break;
        case ActionRemove:
            remove();
            break;
        case ActionEdit:
            ind = currentIndex();
            scrollTo(ind, PositionAtTop);
            emit requestShow();
            edit(ind);
            break;
        case ActionEditor:
            openEditor( selectedText() );
            break;
        case ActionAct:
            emit requestActionDialog(selectedText());
            break;
        case ActionCustom:
            emit requestActionDialog(selectedText(),
                                     &commands[act->property("cmd").toInt()]);
            break;
        }
    }
}

void ClipboardBrowser::createContextMenu()
{
    QAction *act;
    QFont font;

    m_menu = new QMenu(this);

    act = m_menu->addAction( QIcon(":/images/clipboard.svg"),
                           tr("Move to &Clipboard") );
    font = act->font();
    font.setBold(true);
    act->setFont(font);
    act->setData( QVariant(ActionToClipboard) );
    act->setShortcuts(QKeySequence::Copy);

    act = m_menu->addAction( QIcon(":/images/remove.png"), tr("&Remove") );
    act->setShortcut( QString("Delete") );
    act->setData( QVariant(ActionRemove) );

    act = m_menu->addAction( QIcon(":/images/edit.svg"), tr("&Edit") );
    act->setShortcut( QString("F2") );
    act->setData( QVariant(ActionEdit) );

    act = m_menu->addAction( QIcon(":/images/edit.svg"),
                             tr("E&dit with editor") );
    act->setShortcut( QString("Ctrl+E") );
    act->setData( QVariant(ActionEditor) );

    act = m_menu->addAction( QIcon(":/images/action.svg"), tr("&Action...") );
    act->setShortcut( QString("F5") );
    act->setData( QVariant(ActionAct) );

    connect( m_menu, SIGNAL(triggered(QAction*)),
            this, SLOT(contextMenuAction(QAction*)) );
    connect( m_menu, SIGNAL(aboutToShow()),
             this, SLOT(updateContextMenu()) );
}

bool ClipboardBrowser::isFiltered(int row) const
{
    QString text = m->data(m->index(row), Qt::EditRole).toString();
    return m_last_filter.indexIn(text) == -1;
}

void ClipboardBrowser::updateContextMenu()
{
    int i, len;
    QString text = selectedText();
    QAction *act;
    QList<QAction *> actions = m_menu->actions();

    // remove old actions
    for( i = 0, len = actions.size(); i<len && !actions[i]->isSeparator(); ++i );
    for( ; i<len; ++i )
        m_menu->removeAction(actions[i]);

    // add custom commands to menu
    if ( !commands.isEmpty() ) {
        m_menu->addSeparator();
        i = 0;
        foreach(const Command &command, commands) {
            if ( !command.cmd.isEmpty() && !command.name.isEmpty() &&
                 command.re.indexIn(text) != -1 ) {
                act = m_menu->addAction( QIcon(command.icon), command.name );
                act->setData( QVariant(ActionCustom) );
                act->setProperty("cmd", i);
                if ( !command.shortcut.isEmpty() )
                    act->setShortcut( command.shortcut );
            }
            ++i;
        }
    }
}

void ClipboardBrowser::contextMenuEvent(QContextMenuEvent *event)
{
    if ( !selectedIndexes().isEmpty() )
        m_menu->exec( event->globalPos() );
}

void ClipboardBrowser::paintEvent(QPaintEvent *event)
{
    d->setDryPaint(true);
    m_sizeHintChanged = false;
    QListView::paintEvent(event);
    d->setDryPaint(false);

    if (!m_sizeHintChanged)
        QListView::paintEvent(event);
}

void ClipboardBrowser::openEditor(const QString &text)
{
    QEditor *editor = new QEditor(text, m_editor);

    connect( editor, SIGNAL(fileModified(const QString &)),
            this, SLOT(itemModified(const QString &)) );

    connect( editor, SIGNAL(closed(QEditor *)),
            this, SLOT(closeEditor(QEditor *)) );

    connect( this, SIGNAL(closeAllEditors()), editor, SLOT(close()) );

    if ( !editor->start() )
        closeEditor(editor);
}

void ClipboardBrowser::addItems(const QStringList &items)
{
    for(int i=items.count()-1; i>=0; --i) {
        add(items[i], true);
    }
}

void ClipboardBrowser::itemModified(const QString &str)
{
    // add new item
    if ( !str.isEmpty() ) {
        add(str);
    }
}

void ClipboardBrowser::filterItems(const QString &str)
{
    if (m_last_filter.pattern() == str)
        return;
    m_last_filter = QRegExp(str, Qt::CaseInsensitive);

    // if search string empty: all items visible
    d->setSearch(m_last_filter);
    if ( str.isEmpty() ) {
        emit hideSearch();
    }

    // hide filtered items
    reset();
    int first = -1;
    for(int i = 0; i < m->rowCount(); ++i) {
        if ( isFiltered(i) )
            setRowHidden(i, true);
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

void ClipboardBrowser::moveToClipboard(int i)
{
    m->move(i,0);
    updateClipboard();
    scrollTo( currentIndex() );
}

void ClipboardBrowser::newItem()
{
    // new text will allocate more space (lines) for editor
    add( QString(), true );
    selectionModel()->clearSelection();
    setCurrent(0);
    edit( index(0) );
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    // if editing item, use default key action
    if ( state() == QAbstractItemView::EditingState ) {
        QListView::keyPressEvent(event);
    }
    // CTRL
    else if ( event->modifiers() == Qt::ControlModifier ) {
        int key = event->key();
        switch ( key ) {
        // move items
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_End:
        case Qt::Key_Home:
            if ( m->moveItems(selectedIndexes(), key) )
                updateClipboard();
            scrollTo( currentIndex() );
            break;

        // cycle formats
        case Qt::Key_Left:
            m->previousFormat( currentIndex().row() );
            break;
        case Qt::Key_Right:
            m->nextFormat( currentIndex().row() );
            break;

        default:
            updateContextMenu();
            QListView::keyPressEvent(event);
            break;
        }
    }
    else {
        int key = event->key();
        switch ( key ) {
        // navigation
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
        case Qt::Key_Home:
        case Qt::Key_End:
            /* If item height is bigger than viewport height
             * it's possible that wrong item is selected.
             */
            if (key == Qt::Key_PageDown)
                setCurrent(currentIndex().row()+2);
            else if (key == Qt::Key_PageUp)
                setCurrent(currentIndex().row()-2);

            QListView::keyPressEvent(event);
            break;

        default:
            // allow user defined shortcuts
            updateContextMenu();
            QListView::keyPressEvent(event);
            // search
            event->ignore();
            break;
        }
    }
}

void ClipboardBrowser::setCurrent(int row, bool cycle, bool selection)
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
    if ( isRowHidden(i) )
        return;

    QModelIndex ind = index(i);
    if (selection) {
        QItemSelectionModel *sel = selectionModel();
        if ( sel->isSelected(ind) && sel->isSelected(currentIndex()) )
            sel->setCurrentIndex(currentIndex(),
                QItemSelectionModel::Deselect);
        sel->setCurrentIndex(ind,
            QItemSelectionModel::Select);
    }
    else
        setCurrentIndex(ind);

    scrollTo(ind); // ensure visible
}

ClipboardItem *ClipboardBrowser::at(int row) const
{
    return m->at(row);
}

void ClipboardBrowser::remove()
{
    QItemSelectionModel *sel = selectionModel();
    QModelIndexList list = sel->selectedIndexes();

    if ( !list.isEmpty() ) {
        int i;
        bool need_sync = false;

        do {
            i = list.at(0).row();
            if ( i == 0 )
                need_sync = true;
            m->removeRow(i);
            list = sel->selectedIndexes();
        } while( !list.isEmpty() );

        // select next
        setCurrent(i);

        if (need_sync)
            updateClipboard();
    }
}

bool ClipboardBrowser::add(const QString &txt, bool force)
{
    QMimeData *data = new QMimeData;
    data->setText(txt);
    return add(data, force);
}

bool ClipboardBrowser::add(QMimeData *data, bool force)
{
    if (!force) {
        // don't add if new data is same as first item
        if ( *m->at(0) == *data ) {
            delete data;
            return false;
        }

        // commands
        bool ignore = false;
        foreach(const Command &c, commands) {
            if (c.automatic || c.ignore || !c.tab.isEmpty()) {
                QString text = data->text();
                if (c.re.indexIn(text) != -1) {
                    if (c.automatic)
                        emit requestActionDialog(text, &c);
                    if (c.ignore)
                        ignore = true;
                    if (!c.tab.isEmpty())
                        emit addToTab(data, c.tab);
                }
            }
        }
        if (ignore) {
            delete data;
            return false;
        }
    }

    // create new item
    m->insertRow(0);
    QModelIndex ind = index(0);
    m->setData(ind, data);

    // filter item
    if ( isFiltered(0) )
        setRowHidden(0, true);

    // list size limit
    if ( m->rowCount() > m_maxitems )
        m->removeRow( m->rowCount() - 1 );

    // save history after 2 minutes
    saveItems(120000);

    return true;
}

bool ClipboardBrowser::add(const ClipboardItem &item, bool force)
{
    return add( cloneData(*item.data()), force );
}

void ClipboardBrowser::loadSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    setStyleSheet( cm->styleSheet() );
    d->setStyleSheet( cm->styleSheet() );

    // restore configuration
    m_editor = cm->value("editor").toString();

    m_maxitems = cm->value("maxitems").toInt();
    m->setMaxItems(m_maxitems);
    m->setFormats( cm->value("formats").toString() );
    m->setMaxImageSize( cm->value("max_image_width").toInt(),
                        cm->value("max_image_height").toInt() );

    // commands
    commands = cm->commands();

    // update menu
    updateContextMenu();
}

void ClipboardBrowser::loadItems()
{
    timer_save.stop();
    ConfigurationManager::instance()->loadItems(*m, m_id);
    setCurrentIndex( QModelIndex() );
}

void ClipboardBrowser::saveItems(int msec)
{
    timer_save.stop();
    if (msec>0) {
        timer_save.start(msec);
        return;
    }

    ConfigurationManager::instance()->saveItems(*m, m_id);
}

void ClipboardBrowser::purgeItems()
{
    ConfigurationManager::instance()->removeItems(m_id);
    timer_save.stop();
}

const QString ClipboardBrowser::selectedText() const
{
    QString result;
    QItemSelectionModel *sel = selectionModel();

    QModelIndexList list = sel->selectedIndexes();
    foreach(QModelIndex ind, list) {
        if ( !result.isEmpty() )
            result += QString('\n');
        result += itemText(ind);
    }

    return result;
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

const QMimeData *ClipboardBrowser::itemData(int i) const
{
    return m->mimeData( i>=0 ? i : currentIndex().row() );
}

void ClipboardBrowser::checkClipboard(QClipboard::Mode, QMimeData *data)
{
    add(data);
}

void ClipboardBrowser::updateClipboard()
{
    if ( m_update && m->rowCount() > 0 )
        emit changeClipboard(m->at(0));
}

void ClipboardBrowser::realDataChanged(const QModelIndex &a, const QModelIndex &)
{
    if ( a.row() == 0 )
        updateClipboard();
}

void ClipboardBrowser::sizeHintChanged()
{
    m_sizeHintChanged = true;
}

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
#include <QMenu>
#include <QMimeData>
#include <unistd.h> //usleep
#include <actiondialog.h>
#include "itemdelegate.h"
#include "clipboardmodel.h"
#include "configurationmanager.h"
#include "client_server.h"
#include <qtlocalpeer.h>

ClipboardBrowser::ClipboardBrowser(QWidget *parent) : QListView(parent),
    m_monitor(NULL), m_update(false), menu(NULL)
{
    setLayoutMode(QListView::Batched);
    setBatchSize(20);

    // delegate for rendering and editing items
    d = new ItemDelegate(this);
    setItemDelegate(d);

    // set new model
    m = new ClipboardModel();
    QItemSelectionModel *old_model = selectionModel();
    setModel(m);
    delete old_model;

    connect( m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             d, SLOT(rowsRemoved(QModelIndex,int,int)) );
    connect( m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             d, SLOT(rowsInserted(QModelIndex, int, int)) );
    connect( m, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
             d, SLOT(rowsMoved(QModelIndex, int, int, QModelIndex, int)) );
    connect( m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             d, SLOT(dataChanged(QModelIndex,QModelIndex)));

    connect( this, SIGNAL(doubleClicked(QModelIndex)),
            SLOT(moveToClipboard(QModelIndex)));

    // ScrollPerItem doesn't work well with hidden items
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    setAttribute(Qt::WA_MacShowFocusRect, 0);
}

ClipboardBrowser::~ClipboardBrowser()
{
    stopMonitoring();
    emit closeAllEditors();
}

void ClipboardBrowser::monitorStateChanged(QProcess::ProcessState newState)
{
    if (newState == QProcess::NotRunning) {
        qDebug( tr("Clipboard Monitor ERROR: Clipboard monitor crashed! (%s)").toLocal8Bit(),
                m_monitor->errorString().toLocal8Bit().constData() );
        startMonitoring();
    } else if (newState == QProcess::Starting) {
        qDebug( tr("Clipboard Monitor: Starting").toLocal8Bit() );
    } else if (newState == QProcess::Running) {
        qDebug( tr("Clipboard Monitor: Started").toLocal8Bit() );
    }
}

void ClipboardBrowser::monitorStandardError()
{
    qDebug( tr("Clipboard Monitor error output:").toLocal8Bit() );
    qDebug( m_monitor->readAllStandardError() );
}

void ClipboardBrowser::stopMonitoring()
{
    if (m_monitor) {
        qDebug( tr("Clipboard Monitor: Terminating").toLocal8Bit() );
        m_monitor->disconnect( SIGNAL(stateChanged(QProcess::ProcessState)) );

        // send "exit" command to clipboard monitor process
        QString msg;
        DataList args;
        args << QByteArray("exit");
        serialize_args(args, msg);

        QtLocalPeer peer( NULL, QString("CopyQmonitor") );
        if ( peer.sendMessage(msg, 1000) ) {
            m_monitor->waitForFinished(1000);
        }

        if ( m_monitor->state() != QProcess::NotRunning ) {
            qDebug( tr("Clipboard Monitor ERROR: Command 'exit' unsucessful!").toLocal8Bit() );
            m_monitor->terminate();
            m_monitor->waitForFinished(1000);
        }

        if ( m_monitor->state() != QProcess::NotRunning ) {
            qDebug( tr("Clipboard Monitor ERROR: Cannot terminate process!").toLocal8Bit() );
            m_monitor->kill();
        }

        if ( m_monitor->state() != QProcess::NotRunning ) {
            qDebug( tr("Clipboard Monitor ERROR: Cannot kill process!!!").toLocal8Bit() );
        } else {
            qDebug( tr("Clipboard Monitor: Terminated").toLocal8Bit() );
        }

        delete m_monitor;
        m_monitor = NULL;
    }
    m_update = false;
}

void ClipboardBrowser::startMonitoring()
{
    if ( !m_monitor ) {
        m_monitor = new QProcess;
        connect( m_monitor, SIGNAL(stateChanged(QProcess::ProcessState)),
                 this, SLOT(monitorStateChanged(QProcess::ProcessState)) );
        connect( m_monitor, SIGNAL(readyReadStandardError()),
                 this, SLOT(monitorStandardError()) );
    }

    if ( m_monitor->state() == QProcess::NotRunning ) {
        m_monitor->start( QCoreApplication::arguments().at(0),
                          QStringList() << "monitor",
                          QProcess::NotOpen );
        m_monitor->waitForStarted(2000);
    }
    m_update = true;
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
    const QString text = act->text();

    if (text == tr("Move to &clipboard")) {
        moveToClipboard( this->currentIndex() );
        setCurrent(0);
    } else if (text == tr("&Remove")) {
        remove();
    } else if (text == tr("&Edit")) {
        QModelIndex ind = currentIndex();
        scrollTo(ind, PositionAtTop);
        edit(ind);
    } else if (text == tr("E&dit with editor")) {
        openEditor();
    } else if (text == tr("&Action...")) {
        emit requestActionDialog(-1);
    } else {
        const ConfigurationManager::Command *c = &commands[text];
        emit requestActionDialog(-1, c);
    }
}

void ClipboardBrowser::setMenu(QMenu *menu)
{
     this->menu = menu;
     connect( menu, SIGNAL(triggered(QAction*)),
              this, SLOT(contextMenuAction(QAction*)) );
}

void ClipboardBrowser::updateMenuItems()
{
    menu->clear();

    QAction *act = menu->addAction( QIcon(":/images/clipboard.svg"), tr("Move to &clipboard") );
    QFont font( act->font() );
    font.setBold(true);
    act->setFont(font);

    act = menu->addAction( QIcon(":/images/remove.png"), tr("&Remove") );
    act->setShortcut( QString("Delete") );
    act = menu->addAction( QIcon(":/images/edit.svg"), tr("&Edit") );
    act->setShortcut( QString("F2") );
    act = menu->addAction( QIcon(":/images/edit.svg"), tr("E&dit with editor") );
    act->setShortcut( QString("Ctrl+E") );
    act = menu->addAction( QIcon(":/images/action.svg"), tr("&Action...") );
    act->setShortcut( QString("F5") );

    // add custom commands to menu
    if ( !commands.isEmpty() )
        menu->addSeparator();

    QString text = selectedText();
    foreach( QString name, commands.keys() ) {
        const ConfigurationManager::Command *command = &commands[name];
        if ( command->re.indexIn(text) != -1 ) {
            act = menu->addAction(command->icon, name);
            if ( !command->shortcut.isEmpty() )
                act->setShortcut( command->shortcut );
        }
    }
}

void ClipboardBrowser::contextMenuEvent(QContextMenuEvent *event)
{
    if ( this->selectedIndexes().isEmpty() )
        return;

    if (menu) {
        updateMenuItems();
        menu->exec( event->globalPos() );
    }
}

void ClipboardBrowser::selectionChanged(const QItemSelection &,
                                        const QItemSelection &)
{
    updateMenuItems();
}

void ClipboardBrowser::openEditor()
{
    QEditor *editor = new QEditor(selectedText(), m_editor);

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
        add(items[i]);
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
    // if search string empty: all items visible
    if ( str.isEmpty() ) {
        m->setSearch();
        emit hideSearch();
    }
    else {
        // if search string is a number N: highlight Nth item
        bool ok;
        int n = str.toInt(&ok);
        if (ok && n >= 0 && n < m->rowCount()) {
            m->setSearch(n);
            setCurrent(n);
            return;
        } else {
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

void ClipboardBrowser::moveToClipboard(int i)
{
    if ( i > 0 ) {
        m->move(i,0);
        updateClipboard();
        scrollTo( currentIndex() );
        update();
    }
}

void ClipboardBrowser::newItem()
{
    // new text will allocate more space (lines) for editor
    add( QString(), false );
    selectionModel()->clearSelection();
    setCurrent(0);
    edit( index(0) );
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    /*
    int last, current;
    QModelIndex newindex;
    QItemSelectionModel::SelectionFlags selflags;
    QModelIndexList selected;
    */

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
            /*
            last = m->rowCount()-1;
            current = currentIndex().row();
            if (key == Qt::Key_Up) {
                newindex = m->index(current == 0 ? last : current-1);
            } else if (key == Qt::Key_Down) {
                newindex = m->index(current == last ? 0 : current+1);
            }
            if ( newindex.isValid() ) {
                selflags = event->modifiers() == Qt::ShiftModifier ?
                           QItemSelectionModel::Toggle :
                           QItemSelectionModel::ClearAndSelect;

                selected = selectedIndexes();

                selectionModel()->setCurrentIndex(newindex, selflags);

                foreach( QModelIndex ind, selected) {
                    update(ind);
                }
            } else {
                QListView::keyPressEvent(event);
            }
            */
            QListView::keyPressEvent(event);
            break;

        default:
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

bool ClipboardBrowser::add(const QString &txt, bool ignore_empty)
{
    QMimeData *data = new QMimeData;
    data->setText(txt);
    return add(data, ignore_empty);
}

bool ClipboardBrowser::add(QMimeData *data, bool ignore_empty)
{
    QStringList formats = data->formats();

    // ignore empty
    if ( ignore_empty && (formats.isEmpty() || (data->hasText() && data->text().isEmpty())) ) {
        return false;
    }

    // don't add if new data is same as first item
    if ( m->rowCount() ) {
        QMimeData *olddata = m->mimeData(0);
        QStringList oldformats = olddata->formats();
        if ( oldformats == formats &&
             olddata->data(oldformats.at(0)) == data->data(formats.at(0)) ) {
            return false;
        }
    }

    // create new item
    m->insertRow(0);
    QModelIndex ind = index(0);
    m->setData(ind, data);

    // filter item
    if ( m->isFiltered(0) )
        setRowHidden(0,true);

    // list size limit
    if ( m->rowCount() > m_maxitems )
        m->removeRow( m->rowCount() - 1 );

    // save history after 2 minutes
    saveItems(120000);

    return true;
}

void ClipboardBrowser::loadSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    setStyleSheet( cm->styleSheet() );
    d->setStyleSheet( cm->styleSheet() );

    // restore configuration
    m_editor = cm->value(ConfigurationManager::Editor).toString();
    d->setItemFormat( cm->value(ConfigurationManager::ItemHTML).toString());

    m_maxitems = cm->value(ConfigurationManager::MaxItems).toInt();
    m->setMaxItems( m_maxitems );
    m->setFormats( cm->value(ConfigurationManager::Priority).toString() );

    m_callback.clear();

    //callback program
    // FIXME: arguments that contain spaces aren't parsed well
    m_callback_args = cm->value(ConfigurationManager::Callback).toString().split(" ");
    if( !m_callback_args.isEmpty() ) {
        m_callback = m_callback_args[0];
        m_callback_args.pop_front();
    }

    // commands
    commands = cm->commands();

    update();

    // restart clipboard monitor to load its configuration
    if ( isMonitoring() ) {
        stopMonitoring();
        startMonitoring();
    }
}

void ClipboardBrowser::loadItems()
{
    timer_save.stop();

    // restore items
    m->clear();

    ConfigurationManager::instance()->loadItems(*m);
    setCurrentIndex( QModelIndex() );

    // performance:
    // force the delegate to calculate the size of each item
    // -- this will save some time when drawing the list for the fist time
    //sizeHintForColumn(0);
}

void ClipboardBrowser::saveItems(int msec)
{
    timer_save.stop();
    if (msec>0) {
        timer_save.singleShot( msec, this, SLOT(saveItems()) );
        return;
    }

    ConfigurationManager::instance()->saveItems(*m);
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

    if ( result.isEmpty() )
        result = itemText(0);

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

void ClipboardBrowser::runCallback() const
{
    // run callback program on clipboard contents
    if ( m_callback.isEmpty() )
        return;
    QProcess::startDetached( m_callback, m_callback_args + QStringList(itemText(0)) );
}

void ClipboardBrowser::checkClipboard(QClipboard::Mode, QMimeData *data)
{
    add(data);
}

void ClipboardBrowser::updateClipboard()
{
    if ( m->rowCount() == 0 )
        return;

    // TODO: first item -> clipboard
    if ( m_update ) {
        QString msg;
        DataList args;
        const QMimeData *data = m->mimeData(0);

        args << QByteArray("write");
        foreach ( QString mime, data->formats() ) {
            args << mime.toLocal8Bit() << data->data(mime);
        }
        serialize_args(args, msg);

        QtLocalPeer peer( NULL, QString("CopyQmonitor") );
        // is monitor running?
        if ( peer.isClient() ) {
            if ( !peer.sendMessage(msg, 2000) ) {
                // monitor is not responding -> restart it
                m_monitor->kill();
                // TODO: send the data again
            }
        }
    }

    runCallback();
}

void ClipboardBrowser::dataChanged(const QModelIndex &a, const QModelIndex &)
{
    if ( a.row() == 0 && m->search()->isEmpty() ) {
        updateClipboard();
    }
    update(a);
}

void ClipboardBrowser::resizeEvent(QResizeEvent *e)
{
    QListView::resizeEvent(e);
    d->invalidateSizes();
    update();
}

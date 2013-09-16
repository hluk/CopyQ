/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "common/client_server.h"
#include "common/contenttype.h"
#include "gui/clipboarddialog.h"
#include "gui/configtabappearance.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "item/clipboarditem.h"
#include "item/clipboardmodel.h"
#include "item/itemdelegate.h"
#include "item/itemeditor.h"
#include "item/itemeditorwidget.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/serialize.h"

#include <QKeyEvent>
#include <QPushButton>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>

namespace {

const char propertyActionIndex[] = "CopyQ_action_index";
const char propertyActionInOwnMenu[] = "CopyQ_action_in_own_menu";

bool alphaSort(const ClipboardModel::ComparisonItem &lhs,
                     const ClipboardModel::ComparisonItem &rhs)
{
    return lhs.second->text().localeAwareCompare( rhs.second->text() ) < 0;
}

bool reverseSort(const ClipboardModel::ComparisonItem &lhs,
                        const ClipboardModel::ComparisonItem &rhs)
{
    return lhs.first > rhs.first;
}

} // namespace

ClipboardBrowserShared::ClipboardBrowserShared()
    : editor()
    , maxItems(100)
    , textWrap(true)
    , commands()
    , viMode(false)
    , saveOnReturnKey(false)
    , moveItemOnReturnKey(false)
    , minutesToExpire(0)
{
}

void ClipboardBrowserShared::loadFromConfiguration()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    editor = cm->value("editor").toString();
    maxItems = cm->value("maxitems").toInt();
    textWrap = cm->value("text_wrap").toBool();
    commands = cm->commands();
    viMode = cm->value("vi").toBool();
    saveOnReturnKey = !cm->value("edit_ctrl_return").toBool();
    moveItemOnReturnKey = cm->value("move").toBool();
    minutesToExpire = cm->value("expire_tab").toInt();
}

ClipboardBrowser::Lock::Lock(ClipboardBrowser *self) : c(self)
{
    m_autoUpdate = c->autoUpdate();
    c->setAutoUpdate(false);
    c->setUpdatesEnabled(false);
}

ClipboardBrowser::Lock::~Lock()
{
    c->setAutoUpdate(m_autoUpdate);
    c->setUpdatesEnabled(true);
}

ClipboardBrowser::ClipboardBrowser(QWidget *parent, const ClipboardBrowserSharedPtr &sharedData)
    : QListView(parent)
    , m_loaded(false)
    , m_id()
    , m_lastFilter()
    , m_update(false)
    , m( new ClipboardModel(this) )
    , d( new ItemDelegate(this) )
    , m_timerSave( new QTimer(this) )
    , m_timerScroll( new QTimer(this) )
    , m_timerUpdate( new QTimer(this) )
    , m_timerExpire(NULL)
    , m_menu(NULL)
    , m_save(true)
    , m_editor(NULL)
    , m_sharedData(sharedData ? sharedData : ClipboardBrowserSharedPtr(new ClipboardBrowserShared))
    , m_loadButton(NULL)
{
    setLayoutMode(QListView::Batched);
    setBatchSize(1);
    setFrameShape(QFrame::NoFrame);
    setTabKeyNavigation(false);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setWrapping(false);
    setLayoutMode(QListView::SinglePass);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSpacing(5);

    initSingleShotTimer(m_timerSave, 30000, SLOT(saveItems()));
    initSingleShotTimer(m_timerScroll, 50);
    initSingleShotTimer(m_timerUpdate, 0, SLOT(updateCurrentPage()));

    // delegate for rendering and editing items
    setItemDelegate(d);

    // set new model
    QItemSelectionModel *old_model = selectionModel();
    setModel(m);
    delete old_model;

    connect( m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             d, SLOT(rowsRemoved(QModelIndex,int,int)) );
    connect( m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             d, SLOT(rowsInserted(QModelIndex, int, int)) );
    connect( m, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
             d, SLOT(rowsMoved(QModelIndex, int, int, QModelIndex, int)) );

    // save if data in model changed
    connect( m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(onDataChanged(QModelIndex,QModelIndex)) );
    connect( m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             SLOT(delayedSaveItems()) );
    connect( m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(delayedSaveItems()) );
    connect( m, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
             SLOT(delayedSaveItems()) );

    // update on change
    connect( d, SIGNAL(rowSizeChanged(int)),
             SLOT(onRowSizeChanged(int)) );
    connect( m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             SLOT(updateCurrentPage()) );
    connect( m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(updateCurrentPage()) );
    connect( m, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
             SLOT(updateCurrentPage()) );
    connect( verticalScrollBar(), SIGNAL(valueChanged(int)),
             SLOT(updateCurrentPage()) );
    connect( verticalScrollBar(), SIGNAL(rangeChanged(int,int)),
             SLOT(updateCurrentPage()), Qt::QueuedConnection );

    // ScrollPerItem doesn't work well with hidden items
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    setAttribute(Qt::WA_MacShowFocusRect, 0);

    setAcceptDrops(true);
}

ClipboardBrowser::~ClipboardBrowser()
{
    d->invalidateCache();
    if ( m_timerSave->isActive() )
        saveItems();
}


void ClipboardBrowser::closeExternalEditor(QObject *editor)
{
    editor->disconnect(this);
    disconnect(editor);
    delete editor;
}

void ClipboardBrowser::contextMenuAction()
{
    Q_ASSERT(sender() != NULL);
    QObject *obj = sender();

    QVariant actionData = obj->property(propertyActionIndex);
    Q_ASSERT( actionData.isValid() );

    int i = actionData.toInt();
    if (i < 0 || i >= m_sharedData->commands.size())
        return;

    Command cmd = m_sharedData->commands[i];
    if ( cmd.outputTab.isEmpty() )
        cmd.outputTab = m_id;

    bool isContextMenuAction = obj->property(propertyActionInOwnMenu).toBool();
    const QModelIndexList selected = selectedIndexes();

    const QMimeData *data = isContextMenuAction ? getSelectedItemData() : clipboardData();
    QMimeData textData;
    if (data == NULL)
        textData.setText(selectedText());

    if ( !cmd.cmd.isEmpty() ) {
        if (isContextMenuAction && cmd.transform) {
            foreach (const QModelIndex &index, selected) {
                const QMimeData *data = itemData( index.row() );

                if (cmd.input == mimeItems) {
                    QMimeData data2;
                    data2.setData( mimeItems, serializeData(*data) );
                    emit requestActionDialog(data2, cmd, index);
                } else if ( cmd.input.isEmpty() || data->hasFormat(cmd.input) ) {
                    emit requestActionDialog(*data, cmd, index);
                }
            }
        } else {
            if (data != NULL) {
                emit requestActionDialog(*data, cmd);
            } else {
                emit requestActionDialog(textData, cmd);
            }
        }
    }

    if ( !cmd.tab.isEmpty() && cmd.tab != getID() ) {
        for (int i = selected.size() - 1; i >= 0; --i) {
            const QMimeData *data = itemData( selected[i].row() );
            if (data != NULL)
                emit addToTab(*data, cmd.tab);
        }
    }

    if (cmd.remove) {
        int current = -1;
        foreach (const QModelIndex &index, selected) {
            if (index.isValid()) {
                int row = index.row();
                removeRow(row);
                if (current == -1 || current > row)
                    current = row;
            }
        }
        if ( !currentIndex().isValid() )
            setCurrent(current);
        if ( !selectionModel()->hasSelection() )
            setCurrentIndex( currentIndex() );
    }

    if (isContextMenuAction && cmd.hideWindow)
        emit requestHide();
}

void ClipboardBrowser::createContextMenu()
{
    if (m_menu == NULL)
        return;

    foreach ( QAction *action, m_menu->actions() )
        delete action;

    if (editing())
        return;

    updateContextMenu();
}

bool ClipboardBrowser::isFiltered(const QModelIndex &index, int role) const
{
    QString text = m->data(index, role).toString();
    return m_lastFilter.indexIn(text) == -1;
}

bool ClipboardBrowser::isFiltered(int row) const
{
    QModelIndex ind = m->index(row);
    return isFiltered(ind, Qt::EditRole) && isFiltered(ind, contentType::notes);
}

bool ClipboardBrowser::hideFiltered(int row)
{
    bool hide = isFiltered(row);
    setRowHidden(row, hide);
    d->setRowVisible(row, !hide);

    return hide;
}

bool ClipboardBrowser::startEditor(QObject *editor)
{
    connect( editor, SIGNAL(fileModified(QByteArray,QString)),
            this, SLOT(itemModified(QByteArray,QString)) );

    connect( editor, SIGNAL(closed(QObject *)),
             this, SLOT(closeExternalEditor(QObject *)) );

    bool retVal = false;
    bool result = QMetaObject::invokeMethod( editor, "start", Qt::DirectConnection,
                                             Q_RETURN_ARG(bool, retVal) );

    if (!retVal || !result) {
        closeExternalEditor(editor);
        return false;
    }

    return true;
}

void ClipboardBrowser::copyItemToClipboard(int d)
{
    QModelIndex ind = currentIndex();
    int row = ind.isValid() ? ind.row() : 0;
    row = qMax(0, row + d);

    if (row < m->rowCount()) {
        clearSelection();
        setCurrentIndex(index(row));
        updateClipboard(row);
    }
}

void ClipboardBrowser::preload(int minY, int maxY)
{
    ClipboardBrowser::Lock lock(this);

    QModelIndex ind;
    int i = 0;
    int y = spacing();
    const int s = 2 * spacing();
    const int offset = verticalOffset();

    // Start preloading from current item and keep relative scroll offset.
    const int currentY = visualRect(currentIndex()).y();
    bool currentIsVisible = currentY > 0 && currentY < viewport()->contentsRect().height();

    // Find first index to preload.
    forever {
        ind = index(i);
        if ( !ind.isValid() )
            return;

        if ( !isIndexHidden(ind) ) {
            const int h = d->sizeHint(ind).height();
            if (y + h >= offset)
                break;
            y += s + h;
        }

        d->setRowVisible(i, false);

        ++i;
    }

    // Absolute to relative.
    y -= offset;

    // Preload items backwards and correct scroll offset.
    if (i > 0) {
        forever {
            const int lastIndex = i;
            for ( ind = index(--i); ind.isValid() && isIndexHidden(ind); ind = index(--i) ) {}

            if ( !ind.isValid() ) {
                i = lastIndex;
                ind = index(i);
                break;
            }

            // Fetch item.
            const int h = d->cache(ind)->widget()->height();

            // Done?
            y -= s + h;
            if (y + h < minY)
                break;
        }
    }

    bool update = false;
    bool lastToPreload = false;

    // Render visible items forwards.
    forever {
        // Fetch item.
        const int h = d->cache(ind)->widget()->height();

        // Correct position.
        if (y != d->cache(ind)->widget()->y())
            d->updateRowPosition( i, QPoint(spacing(), y) );

        d->setRowVisible(i, true);

        // Re-layout items afterwards if item position changed.
        if (!update && y != visualRect(ind).y()) {
            currentIsVisible = currentIsVisible && y < 0;
            update = true;
        }

        // Done?
        y += s + h;
        if (y > maxY) {
            if (lastToPreload)
                break;
            lastToPreload = true; // One more item to preload.
        }

        for ( ind = index(++i); ind.isValid() && isIndexHidden(ind); ind = index(++i) ) {}

        if ( !ind.isValid() )
            break;
    }

    // Hide the rest.
    for ( ++i ; i < m->rowCount(); ++i )
        d->setRowVisible(i, false);

    if (update) {
        scheduleDelayedItemsLayout();
        if (currentIsVisible) {
            scrollTo(currentIndex(), currentY <= s ? QAbstractItemView::PositionAtTop
                                                   : QAbstractItemView::PositionAtCenter);
        }
    }
}

void ClipboardBrowser::setEditorWidget(ItemEditorWidget *editor)
{
    bool active = editor != NULL;

    if (m_editor != editor) {
        m_editor = editor;
        if (active) {
            connect( editor, SIGNAL(destroyed()),
                     this, SLOT(onEditorDestroyed()) );
            connect( editor, SIGNAL(save()),
                     this, SLOT(onEditorSave()) );
            connect( editor, SIGNAL(cancel()),
                     this, SLOT(onEditorCancel()) );
            connect( editor, SIGNAL(invalidate()),
                     editor, SLOT(deleteLater()) );
            updateEditorGeometry();
            editor->show();
            editor->setFocus();
        } else {
            setFocus();
        }
    }

    setFocusPolicy(active ? Qt::NoFocus : Qt::StrongFocus);

    setAcceptDrops(!active);

    // Hide scrollbars while editing.
    Qt::ScrollBarPolicy scrollbarPolicy = Qt::ScrollBarAlwaysOff;
    if (!active) {
        const ConfigTabAppearance *appearance = ConfigurationManager::instance()->tabAppearance();
        scrollbarPolicy = appearance->themeValue("show_scrollbars").toBool()
                ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    }
    setVerticalScrollBarPolicy(scrollbarPolicy);
    setHorizontalScrollBarPolicy(scrollbarPolicy);
}

void ClipboardBrowser::editItem(const QModelIndex &index, bool editNotes)
{
    if (!index.isValid())
        return;

    ItemEditorWidget *editor = d->createCustomEditor(this, index, editNotes);
    if (editor != NULL) {
        if ( editor->isValid() )
            setEditorWidget(editor);
        else
            delete editor;
    }
}

void ClipboardBrowser::updateEditorGeometry()
{
    if ( editing() ) {
        const QRect contents = viewport()->contentsRect();
        const QMargins margins = contentsMargins();
        m_editor->setGeometry( contents.translated(margins.left(), margins.top()) );
    }
}

void ClipboardBrowser::initSingleShotTimer(QTimer *timer, int milliseconds, const char *slot)
{
    timer->setSingleShot(true);
    timer->setInterval(milliseconds);
    if (slot != NULL) {
        connect( timer, SIGNAL(timeout()),
                 this, slot );
    }
}

void ClipboardBrowser::restartExpiring()
{
    if ( m_loaded && m_timerExpire != NULL && isHidden() )
        m_timerExpire->start();
}

void ClipboardBrowser::stopExpiring()
{
    if (m_timerExpire != NULL)
        m_timerExpire->stop();
}

void ClipboardBrowser::updateCurrentItem()
{
    const QModelIndex current = currentIndex();
    if ( current.isValid() && d->hasCache(current) ) {
        ItemWidget *item = d->cache(current);
        item->setCurrent(false);
        item->setCurrent( hasFocus() );
    }
}

void ClipboardBrowser::initActions()
{
    createAction( Actions::Item_MoveToClipboard, SLOT(moveToClipboard()) );
    createAction( Actions::Item_ShowContent, SLOT(showItemContent()) );
    createAction( Actions::Item_Remove, SLOT(remove()) );
    createAction( Actions::Item_Edit, SLOT(editSelected()) );
    createAction( Actions::Item_EditNotes, SLOT(editNotes()) );
    createAction( Actions::Item_EditWithEditor, SLOT(openEditor()) );
    createAction( Actions::Item_Action, SLOT(action()) );
    createAction( Actions::Item_NextToClipboard, SLOT(copyNextItemToClipboard()) );
    createAction( Actions::Item_PreviousToClipboard, SLOT(copyPreviousItemToClipboard()) );
}

void ClipboardBrowser::clearActions()
{
    foreach (QAction *action, actions()) {
        action->disconnect(this);
        removeAction(action);
        if (m_menu != NULL)
            m_menu->removeAction(action);
    }

    if (m_menu != NULL) {
        foreach ( QAction *action, m_menu->actions() )
            delete action;
    }
}

QAction *ClipboardBrowser::createAction(Actions::Id id, const char *slot)
{
    ConfigTabShortcuts *shortcuts = ConfigurationManager::instance()->tabShortcuts();
    QAction *act = shortcuts->action(id, this, Qt::WidgetShortcut);
    connect( act, SIGNAL(triggered()), this, slot, Qt::UniqueConnection );
    m_menu->addAction(act);
    return act;
}

void ClipboardBrowser::addCommandsToMenu(QMenu *menu, const QString &text, const QMimeData *data)
{
    if ( m_sharedData->commands.isEmpty() )
        return;

    const QString windowTitle = data == NULL ? QString() : QString::fromUtf8(
                data->data(mimeWindowTitle).data() );

    bool isOwnMenu = menu == m_menu;

    QAction *insertBefore = NULL;

    QSet<QString> availableFormats;
    if (data != NULL) {
        availableFormats = data->formats().toSet();
    } else {
        foreach ( const QModelIndex &ind, selectionModel()->selectedIndexes() )
            availableFormats.unite( itemData(ind.row())->formats().toSet() );
    }

    QList<QAction*> actions;
    QList<QKeySequence> shortcuts;

    for (int i = m_sharedData->commands.size() - 1; i >= 0; --i) {
        const Command &command = m_sharedData->commands[i];

        // Verify that named command is provided and text, MIME type and window title are matched.
        if ( !command.inMenu
            || (command.cmd.isEmpty() && (command.tab.isEmpty() || command.tab == getID()))
            || command.name.isEmpty()
            || command.re.indexIn(text) == -1
            || command.wndre.indexIn(windowTitle) == -1 )
        {
            continue;
        }

        // Verify that data for given MIME is available.
        if (command.input == mimeItems) {
            // Disallow applying action that takes serialized item more times.
            if ( availableFormats.contains(command.output) )
                continue;
        } else if ( !command.input.isEmpty() && !availableFormats.contains(command.input) ) {
            continue;
        }

        QAction *act = new QAction(this);
        menu->insertAction(insertBefore, act);

        act->setProperty(propertyActionIndex, i);
        act->setProperty(propertyActionInOwnMenu, isOwnMenu);

        act->setText( elideText(command.name, font()) );

        IconFactory *iconFactory = ConfigurationManager::instance()->iconFactory();
        act->setIcon( iconFactory->iconFromFile(command.icon) );

        if ( isOwnMenu && !command.shortcut.isEmpty() ) {
            const QKeySequence shortcut = command.shortcut;

            // Disable same shortcuts in commands.
            for (int j = actions.size() - 1; j >= 0; --j) {
                QAction *act = actions[j];
                if ( act->shortcut() == shortcut ) {
                    act->setShortcut(QKeySequence());
                    actions.removeAt(j);
                }
            }

            actions.append(act);
            act->setShortcut(command.shortcut);
            shortcuts.append(shortcut);
        }

        insertBefore = act;
        connect(act, SIGNAL(triggered()), this, SLOT(contextMenuAction()));
    }

    if (isOwnMenu)
        ConfigurationManager::instance()->tabShortcuts()->setDisabledShortcuts(shortcuts);
}

void ClipboardBrowser::setItemData(const QModelIndex &index, QMimeData *data)
{
    if (!m->setData(index, data))
        delete data;
}

void ClipboardBrowser::setSavingEnabled(bool enable)
{
    if (m_save == enable)
        return;

    m_save = enable;
    if (m_save) {
        delayedSaveItems();
    } else {
        m_timerSave->stop();
        ConfigurationManager::instance()->removeItems( getID() );
    }
}

void ClipboardBrowser::updateContextMenu()
{
    if (m_menu == NULL)
        return;

    m_menu->clear();

    initActions();

    m_menu->addSeparator();

    addCommandsToMenu(m_menu, selectedText(), getSelectedItemData());
}

void ClipboardBrowser::onDataChanged(const QModelIndex &a, const QModelIndex &b)
{
    QListView::dataChanged(a, b);
    d->dataChanged(a, b);
    delayedSaveItems();

    bool updateMenu = false;
    const QModelIndexList selected = selectedIndexes();

    // Refilter items.
    for (int i = a.row(); i <= b.row(); ++i) {
        hideFiltered(i);
        if ( !updateMenu && selected.contains(index(i)) )
            updateMenu = true;
    }

    if (updateMenu)
        updateContextMenu();

    updateCurrentPage();
}

void ClipboardBrowser::onRowSizeChanged(int row)
{
    if ( updatesEnabled() && visualRect(index(row)).intersects(viewport()->contentsRect()) ) {
        updateCurrentPage();
        scheduleDelayedItemsLayout();
    }
}

void ClipboardBrowser::updateCurrentPage()
{
    restartExpiring();

    if ( m_timerUpdate->isActive() )
        return; // Update already requested.
    if ( m->isDisabled() )
        return;
    if ( !m_loaded && !m_id.isEmpty() )
        return; // Items not loaded yet.
    if ( !isVisible() )
        return; // Update on showEvent().

    if ( sender() == m_timerUpdate && updatesEnabled() ) {
        m_timerUpdate->stop();
        preload(-2 * spacing(), viewport()->contentsRect().height() + 2 * spacing());
        updateCurrentItem();
    } else {
        m_timerUpdate->start();
    }
}

void ClipboardBrowser::expire()
{
    if ( m_timerSave->isActive() )
        saveItems();

    m_loaded = false;
    clear();
}

void ClipboardBrowser::onEditorDestroyed()
{
    setEditorWidget(NULL);
}

void ClipboardBrowser::onEditorSave()
{
    Q_ASSERT(m_editor != NULL);
    m_editor->commitData(m);
}

void ClipboardBrowser::onEditorCancel()
{
    maybeCloseEditor();
}

void ClipboardBrowser::contextMenuEvent(QContextMenuEvent *event)
{
    if ( m_menu != NULL && !editing() && !selectedIndexes().isEmpty() ) {
        m_menu->exec( event->globalPos() );
        event->accept();
    }
}

void ClipboardBrowser::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);

    if (m_sharedData->textWrap)
        d->setItemMaximumSize( viewport()->contentsRect().size() );

    if (m_loadButton != NULL)
        m_loadButton->resize( event->size() );

    updateEditorGeometry();

    updateCurrentPage();
}

void ClipboardBrowser::showEvent(QShowEvent *event)
{
    stopExpiring();

    if ( !m->isDisabled() )
        loadItems();

    if (!currentIndex().isValid())
        setCurrent(0);
    if ( m->rowCount() > 0 && !d->hasCache(index(0)) )
        scrollToTop();

    QListView::showEvent(event);

    updateCurrentPage();
}

void ClipboardBrowser::hideEvent(QHideEvent *event)
{
    QListView::hideEvent(event);
    restartExpiring();
}

void ClipboardBrowser::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QListView::currentChanged(current, previous);

    if ( previous.isValid() && d->hasCache(previous) ) {
        ItemWidget *item = d->cache(previous);
        item->setCurrent(false);
    }

    updateCurrentItem();
}

void ClipboardBrowser::selectionChanged(const QItemSelection &selected,
                                        const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    updateContextMenu();
}

void ClipboardBrowser::focusInEvent(QFocusEvent *event)
{
    // Always focus active editor instead of list.
    if (editing()) {
        focusNextChild();
    } else {
        QListView::focusInEvent(event);
        updateCurrentItem();
    }
}

void ClipboardBrowser::dragEnterEvent(QDragEnterEvent *event)
{
    dragMoveEvent(event);
}

void ClipboardBrowser::dragLeaveEvent(QDragLeaveEvent *event)
{
    QListView::dragLeaveEvent(event);
    m_dragPosition = QPoint();
    update();
}

void ClipboardBrowser::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
    m_dragPosition = event->pos();
    update();
}

void ClipboardBrowser::dropEvent(QDropEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    const int row = index.isValid() ? index.row() : length();
    add( cloneData(*event->mimeData()), row );
    saveItems();
    m_dragPosition = QPoint();
}

void ClipboardBrowser::paintEvent(QPaintEvent *e)
{
    QListView::paintEvent(e);

    // If dragging an item into list, draw indicator for dropping items.
    if ( !m_dragPosition.isNull() ) {
        const int s = spacing();

        QModelIndex pointedIndex = indexAt(m_dragPosition);
        if ( !pointedIndex.isValid() )
            pointedIndex = indexAt(m_dragPosition + QPoint(0, 2 * s));

        QRect rect;
        if ( pointedIndex.isValid() ) {
            rect = visualRect(pointedIndex);
            rect.translate(0, -s);
        } else if ( length() > 0 ){
            rect = visualRect( index(length() - 1) );
            rect.translate(0, rect.height() + s);
        } else {
            rect = viewport()->rect();
            rect.translate(0, s);
        }

        rect.adjust(8, 0, -8, 0);

        QPainter p(viewport());
        p.setPen( QPen(QColor(255, 255, 255, 150), s) );
        p.setCompositionMode(QPainter::CompositionMode_Difference);
        p.drawLine( rect.topLeft(), rect.topRight() );
    }
}

bool ClipboardBrowser::openEditor()
{
    const QModelIndexList selected = selectionModel()->selectedRows();
    return (selected.size() == 1) ? openEditor( selected.first() )
                                  : openEditor( selectedText().toLocal8Bit() );
}

bool ClipboardBrowser::openEditor(const QByteArray &data, const QString &mime,
                                  const QString &editorCommand)
{
    if ( m->isDisabled() || !m_loaded )
        return false;

    const QString &cmd = editorCommand.isNull() ? m_sharedData->editor : editorCommand;
    if (cmd.isNull())
        return false;

    QObject *editor = new ItemEditor(data, mime, cmd, this);
    return startEditor(editor);
}

bool ClipboardBrowser::openEditor(const QModelIndex &index)
{
    if ( m->isDisabled() || !m_loaded )
        return false;

    ItemWidget *item = d->cache(index);
    QObject *editor = item->createExternalEditor(index, this);
    if (editor == NULL) {
        const QMimeData *data = itemData( index.row() );
        if ( data != NULL && data->hasText() ) {
            editor = new ItemEditor(data->text().toLocal8Bit(), QString("text/plain"),
                                    m_sharedData->editor, this);
        }
    }

    return editor != NULL && startEditor(editor);
}

void ClipboardBrowser::addItems(const QStringList &items)
{
    for(int i=items.count()-1; i>=0; --i) {
        add(items[i]);
    }
}

void ClipboardBrowser::showItemContent()
{
    const QMimeData *data = itemData();
    if (data == NULL)
        return;

    ClipboardDialog *d = new ClipboardDialog(data, this);
    connect( d, SIGNAL(finished(int)), d, SLOT(deleteLater()) );
    d->show();
}

void ClipboardBrowser::removeRow(int row)
{
    if (row < 0 && row >= model()->rowCount())
        return;
    model()->removeRow(row);
}

void ClipboardBrowser::editNotes()
{
    QModelIndex ind = currentIndex();
    if ( !ind.isValid() )
        return;

    scrollTo(ind, PositionAtTop);
    emit requestShow(this);

    editItem(ind, true);
}

void ClipboardBrowser::action()
{
    const QMimeData *data = getSelectedItemData();
    if (data != NULL) {
        emit requestActionDialog(*data);
    } else {
        QMimeData textData;
        textData.setText(selectedText());
        emit requestActionDialog(textData);
    }
}

void ClipboardBrowser::itemModified(const QByteArray &bytes, const QString &mime)
{
    // add new item
    if ( !bytes.isEmpty() ) {
        QMimeData *data = new QMimeData;
        data->setData(mime, bytes);
        add(data);
        updateClipboard(0);
        saveItems();
    }
}

void ClipboardBrowser::filterItems(const QString &str)
{
    if (m_lastFilter.pattern() == str)
        return;
    m_lastFilter = QRegExp(str, Qt::CaseInsensitive);

    // if search string empty: all items visible
    d->setSearch(m_lastFilter);

    // row to select
    int first = str.isEmpty() ? currentIndex().row() : -1;

    // hide filtered items
    for(int i = 0; i < m->rowCount(); ++i) {
        if (!hideFiltered(i) && first == -1)
            first = i;
    }
    // select first visible
    setCurrentIndex( index(first) );
    updateCurrentPage();
}

void ClipboardBrowser::moveToClipboard()
{
    moveToClipboard( currentIndex() );
}

void ClipboardBrowser::moveToClipboard(const QModelIndex &ind)
{
    if ( ind.isValid() )
        moveToClipboard(ind.row());
}

void ClipboardBrowser::moveToClipboard(int i)
{
    int row = i;
    if (m_sharedData->moveItemOnReturnKey) {
        m->move(i,0);
        row = 0;
    }
    if ( autoUpdate() )
        updateClipboard(row);
}

void ClipboardBrowser::editNew(const QString &text)
{
    if ( m->isDisabled() || !m_loaded )
        return;

    bool added = add(text);
    if (!added)
        return;

    selectionModel()->clearSelection();

    // Select edited item even if it's hidden.
    QModelIndex newIndex = index(0);
    setCurrentIndex(newIndex);
    editItem( index(0) );
}

void ClipboardBrowser::copyNextItemToClipboard()
{
    copyItemToClipboard(1);
}

void ClipboardBrowser::copyPreviousItemToClipboard()
{
    copyItemToClipboard(-1);
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    // ignore any input if editing an item
    if ( editing() )
        return;

    // translate keys for vi mode
    if (ConfigurationManager::instance()->value("vi").toBool() && handleViKey(event))
        return;

    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    // CTRL
    if (mods == Qt::ControlModifier) {
        switch ( key ) {
        // move items
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_End:
        case Qt::Key_Home:
            m->moveItems(selectedIndexes(), key);
            scrollTo( currentIndex() );
            break;

        // cycle formats
        case Qt::Key_Left:
        case Qt::Key_Right: {
            QModelIndex index = currentIndex();
            if ( index.isValid() ) {
                if (key == Qt::Key_Left)
                    d->previousItemLoader(index);
                else
                    d->nextItemLoader(index);
            }
            break;
        }

        default:
            QListView::keyPressEvent(event);
            break;
        }
    }
    else {
        switch ( key ) {
        // This fixes few issues with default navigation and item selections.
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
        case Qt::Key_Home:
        case Qt::Key_End: {
            QModelIndex current = currentIndex();
            int row = current.row();
            const int h = viewport()->contentsRect().height();
            int d;

            if (key == Qt::Key_PageDown || key == Qt::Key_PageUp) {
                event->accept();

                // Disallow fast page up/down too keep application responsive.
                if (m_timerScroll->isActive())
                    break;
                m_timerScroll->start();

                d = (key == Qt::Key_PageDown) ? 1 : -1;

                QRect rect = visualRect(current);

                if ( rect.height() > h && d < 0 ? rect.top() < 0 : rect.bottom() > h ) {
                    // Scroll within long item.
                    QScrollBar *v = verticalScrollBar();
                    v->setValue( v->value() + d * v->pageStep() );
                    break;
                }

                if ( row == (d > 0 ? m->rowCount() - 1 : 0) )
                    break; // Nothing to do.

                const int minY = d > 0 ? 0 : -h;
                preload(minY, minY + 2 * h);
                executeDelayedItemsLayout();

                rect = visualRect(current);

                const int s = spacing();
                const int fromY = d > 0 ? qMax(0, rect.bottom()) : qMin(h, rect.y());
                const int y = fromY + d * h;
                QModelIndex ind = indexAt(QPoint(s, y));
                if (!ind.isValid()) {
                    ind = indexAt(QPoint(s, y + 2 * s));
                    if (!ind.isValid())
                        ind = index(d > 0 ? m->rowCount() - 1 : 0);
                }

                QRect rect2 = visualRect(ind);
                if (d > 0 && rect2.y() > h && rect2.bottom() - rect.bottom() > h && row + 1 < ind.row())
                    row = ind.row() - 1;
                else if (d < 0 && rect2.bottom() < 0 && rect.y() - rect2.y() > h && row - 1 > ind.row())
                    row = ind.row() + 1;
                else
                    row = d > 0 ? qMax(current.row() + 1, ind.row()) : qMin(current.row() - 1, ind.row());
            } else {
                if (key == Qt::Key_Up) {
                    --row;
                } else if (key == Qt::Key_Down) {
                    ++row;
                } else {
                    if (key == Qt::Key_End) {
                        row = model()->rowCount() - 1;
                        d = 1;
                    } else {
                        row = 0;
                        d = -1;
                    }

                    for ( ; row != current.row() && isRowHidden(row); row -= d ) {}
                }
            }

            setCurrent(row, false, mods == Qt::ShiftModifier);
            break;
        }

        default:
            // allow user defined shortcuts
            QListView::keyPressEvent(event);
            // search
            event->ignore();
            break;
        }
    }
}

void ClipboardBrowser::setCurrent(int row, bool cycle, bool selection)
{
    QModelIndex prev = currentIndex();
    int cur = prev.row();

    // direction
    int dir = cur <= row ? 1 : -1;

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
        ClipboardBrowser::Lock lock(this);
        QItemSelectionModel *sel = selectionModel();
        for ( int j = prev.row(); j != i + dir; j += dir ) {
            QModelIndex ind = index(j);
            if ( !ind.isValid() )
                break;
            if ( isIndexHidden(ind) )
                continue;

            if ( sel->isSelected(ind) && sel->isSelected(prev) )
                sel->setCurrentIndex(currentIndex(), QItemSelectionModel::Deselect);
            sel->setCurrentIndex(ind, QItemSelectionModel::Select);
            prev = ind;
        }
    } else {
        clearSelection();
        setCurrentIndex(ind);
    }
}

ClipboardItem *ClipboardBrowser::at(int row) const
{
    return m->at(row);
}

void ClipboardBrowser::editSelected()
{
    if ( selectedIndexes().size() > 1 ) {
        editNew( selectedText() );
    } else {
        QModelIndex ind = currentIndex();
        if ( ind.isValid() ) {
            emit requestShow(this);
            editItem(ind);
        }
    }
}

void ClipboardBrowser::remove()
{
    QModelIndexList list = selectedIndexes();
    if ( list.isEmpty() )
        return;

    QList<int> rows;
    rows.reserve( list.size() );

    foreach (const QModelIndex &index, list)
        rows.append( index.row() );

    qSort( rows.begin(), rows.end(), qGreater<int>() );

    foreach (int row, rows) {
        if ( !isRowHidden(row) )
            m->removeRow(row);
    }

    int current = rows.last();

    // select next
    setCurrent(current);
}

void ClipboardBrowser::clear()
{
    m->removeRows(0, m->rowCount());
}

bool ClipboardBrowser::select(uint itemHash, bool moveToTop)
{
    int row = m->findItem(itemHash);
    if (row < 0)
        return false;

    if (moveToTop) {
        m->move(row, 0);
        row = 0;
    }

    setCurrent(row);
    moveToClipboard(row);
    return true;
}

void ClipboardBrowser::sortItems(const QModelIndexList &indexes)
{
    m->sortItems(indexes, &alphaSort);
}

void ClipboardBrowser::reverseItems(const QModelIndexList &indexes)
{
    m->sortItems(indexes, &reverseSort);
}

bool ClipboardBrowser::add(const QString &txt, int row)
{
    QMimeData *data = new QMimeData;
    data->setText(txt);
    return add(data, row);
}

bool ClipboardBrowser::add(QMimeData *data, int row)
{
    QScopedPointer<QMimeData> dataGuard(data);

    if ( m->isDisabled() )
        return false;
    if ( !m_loaded && !m_id.isEmpty() ) {
        loadItems();
        if ( m->isDisabled() || !m_loaded )
            return false;
    }

    // create new item
    int newRow = row < 0 ? m->rowCount() : qMin(row, m->rowCount());
    m->insertRow(newRow);
    QModelIndex ind = index(newRow);
    m->setData( ind, dataGuard.take() );

    // filter item
    if ( isFiltered(newRow) ) {
        setRowHidden(newRow, true);
    } else if ( !editing() && !hasFocus() ) {
        // Select new item if clipboard is not focused and the item is not filtered-out.
        clearSelection();
        setCurrentIndex(ind);
    }

    // list size limit
    if ( m->rowCount() > m_sharedData->maxItems )
        m->removeRow( m->rowCount() - 1 );

    delayedSaveItems();

    return true;
}

bool ClipboardBrowser::add(const ClipboardItem &item, int row)
{
    return add( cloneData(item.data()), row );
}

void ClipboardBrowser::loadSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance();

    cm->tabAppearance()->decorateBrowser(this);

    // restore configuration
    m->setMaxItems(m_sharedData->maxItems);

    setTextWrap(m_sharedData->textWrap);

    d->setSaveOnEnterKey(m_sharedData->saveOnReturnKey);

    createContextMenu();

    updateCurrentPage();

    setEditorWidget(m_editor);

    delete m_timerExpire;
    m_timerExpire = NULL;

    if (m_sharedData->minutesToExpire > 0) {
        m_timerExpire = new QTimer(this);
        initSingleShotTimer( m_timerExpire, 60000 * m_sharedData->minutesToExpire, SLOT(expire()) );
        restartExpiring();
    }
}

void ClipboardBrowser::loadItems()
{
    restartExpiring();

    if ( m_loaded || m_id.isEmpty() )
        return;

    m_timerSave->stop();
    m_loaded = ConfigurationManager::instance()->loadItems(*m, m_id);

    // Show lock button if model is disabled.
    if ( !m->isDisabled() ) {
        delete m_loadButton;
        m_loadButton = NULL;
    } else if (m_loadButton == NULL) {
        m_loadButton = new QPushButton( QString(QChar(IconLock)), this );
        QFont iconFont("FontAwesome", 4 * font().pointSize() );
        m_loadButton->setFont(iconFont);
        m_loadButton->setFlat(true);
        m_loadButton->resize( size() );
        m_loadButton->show();
        connect( m_loadButton, SIGNAL(clicked()),
                 this, SLOT(loadItems()) );
    }
}

bool ClipboardBrowser::saveItems()
{
    m_timerSave->stop();

    if ( m->isDisabled() || !m_loaded || !m_save || m_id.isEmpty() )
        return false;

    ConfigurationManager::instance()->saveItems(*m, m_id);
    return true;
}

void ClipboardBrowser::delayedSaveItems()
{
    if ( m->isDisabled() || !m_loaded || !m_save || m_id.isEmpty() || m_timerSave->isActive() )
        return;

    m_timerSave->start();
}

void ClipboardBrowser::purgeItems()
{
    if ( m_id.isEmpty() )
        return;
    ConfigurationManager::instance()->removeItems(m_id);
    m_timerSave->stop();
}

const QString ClipboardBrowser::selectedText() const
{
    QString result;
    QItemSelectionModel *sel = selectionModel();

    QModelIndexList list = sel->selectedIndexes();
    foreach (const QModelIndex &ind, list) {
        if ( !isIndexHidden(ind) ) {
            if ( !result.isEmpty() )
                result += QString('\n');
            result += itemText(ind);
        }
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
    return m->mimeDataInRow( i>=0 ? i : currentIndex().row() );
}

void ClipboardBrowser::updateClipboard(int row)
{
    if ( row < m->rowCount() )
        emit changeClipboard(m->at(row));
}

QByteArray ClipboardBrowser::itemData(int i, const QString &mime) const
{
    const QMimeData *data = itemData(i);
    if (data == NULL)
        return QByteArray();

    return mime == "?" ? data->formats().join("\n").toUtf8() + '\n' : data->data(mime);
}

void ClipboardBrowser::editRow(int row)
{
    editItem( index(row) );
}

void ClipboardBrowser::redraw()
{
    d->invalidateCache();
    updateCurrentPage();
}

void ClipboardBrowser::setContextMenu(QMenu *menu)
{
    clearActions();
    m_menu = menu;
    createContextMenu();
}

void ClipboardBrowser::setID(const QString &id)
{
    if (m_id == id)
        return;

    const QString oldId = m_id;
    m_id = id;

    ConfigurationManager *c = ConfigurationManager::instance();
    bool saved = saveItems();
    if ( !oldId.isEmpty() ) {
        if (saved)
            c->removeItems(oldId);
        else
            c->moveItems(oldId, id);
    }
}

bool ClipboardBrowser::editing()
{
    return m_editor != NULL;
}

bool ClipboardBrowser::maybeCloseEditor()
{
    if ( editing() ) {
        if ( m_editor->hasChanges() ) {
            int answer = QMessageBox::question( this,
                        tr("Discard Changes?"),
                        tr("Do you really want to <strong>discard changes</strong>?"),
                        QMessageBox::No | QMessageBox::Yes,
                        QMessageBox::No );
            if (answer == QMessageBox::No)
                return false;
        }
        delete m_editor;
    }

    return true;
}

bool ClipboardBrowser::handleViKey(QKeyEvent *event)
{
    bool handle = true;
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    switch ( key ) {
    case Qt::Key_G:
        key = mods & Qt::ShiftModifier ? Qt::Key_End : Qt::Key_Home;
        mods = mods & ~Qt::ShiftModifier;
        break;
    case Qt::Key_J:
        key = Qt::Key_Down;
        break;
    case Qt::Key_K:
        key = Qt::Key_Up;
        break;
    default:
        handle = false;
    }

    if (!handle && mods & Qt::ControlModifier) {
        switch ( key ) {
        case Qt::Key_F:
        case Qt::Key_D:
            key = Qt::Key_PageDown;
            mods = mods & ~Qt::ControlModifier;
            handle = true;
            break;
        case Qt::Key_B:
        case Qt::Key_U:
            key = Qt::Key_PageUp;
            mods = mods & ~Qt::ControlModifier;
            handle = true;
            break;
        }
    }

    if (handle) {
        QKeyEvent event2(QEvent::KeyPress, key, mods, event->text());
        keyPressEvent(&event2);
        event->accept();
    }

    return handle;
}

void ClipboardBrowser::setTextWrap(bool enabled)
{
    d->setItemMaximumSize( enabled ? viewport()->contentsRect().size() : QSize(2048, 2048) );
}

const QMimeData *ClipboardBrowser::getSelectedItemData() const
{
    QModelIndexList selected = selectionModel()->selectedRows();
    return (selected.size() == 1) ? itemData(selected.first().row()) : NULL;
}

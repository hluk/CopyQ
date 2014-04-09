/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "common/action.h"
#include "common/common.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "gui/clipboarddialog.h"
#include "gui/configtabappearance.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "item/clipboarditem.h"
#include "item/clipboardmodel.h"
#include "item/itemdelegate.h"
#include "item/itemeditor.h"
#include "item/itemeditorwidget.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/serialize.h"

#include <QApplication>
#include <QDrag>
#include <QKeyEvent>
#include <QMimeData>
#include <QPushButton>
#include <QProgressBar>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QScrollBar>
#include <QSharedPointer>
#include <QTimer>
#include <QElapsedTimer>

namespace {

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

QModelIndex indexNear(const QListView *view, int offset)
{
    const int s = view->spacing();
    QModelIndex ind = view->indexAt( QPoint(s, offset) );
    if ( !ind.isValid() )
        ind = view->indexAt( QPoint(s, offset + 2 * s) );

    return ind;
}

void updateLoadButtonIcon(QPushButton *loadButton)
{
    QColor color = getDefaultIconColor(*loadButton->parentWidget(), QPalette::Base);
    IconFactory *iconFactory = ConfigurationManager::instance()->iconFactory();
    const int iconSize = 64;
    QIcon icon;
    icon.addPixmap(iconFactory->createPixmap(IconLock, color, iconSize));
    icon.addPixmap(iconFactory->createPixmap(IconUnlockAlt, color, iconSize), QIcon::Active);
    loadButton->setIconSize( QSize(iconSize, iconSize) );
    loadButton->setIcon(icon);
}

bool hasFormat(const QVariantMap &data, const QString &format)
{
    if (format == mimeItems) {
        foreach (const QString &key, data.keys()) {
            if ( !key.startsWith(MIME_PREFIX) )
                return true;
        }
        return false;
    }

    return data.contains(format);
}

class CommandAction : public QAction
{
    Q_OBJECT
public:
    enum Type { ClipboardCommand, ItemCommand };

    explicit CommandAction(const Command &command, Type type, ClipboardBrowser *browser)
        : QAction(browser)
        , m_command(command)
        , m_type(type)
        , m_browser(browser)
    {
        Q_ASSERT(browser);

        if (m_type == ClipboardCommand) {
            m_command.transform = false;
            m_command.hideWindow = false;
        }

        setText( elideText(m_command.name, browser->font()) );

        IconFactory *iconFactory = ConfigurationManager::instance()->iconFactory();
        setIcon( iconFactory->iconFromFile(m_command.icon) );
        if (m_command.icon.size() == 1)
            setProperty( "CopyQ_icon_id", m_command.icon[0].unicode() );

        connect(this, SIGNAL(triggered()), this, SLOT(onTriggered()));

        browser->addAction(this);
    }

signals:
    void triggerCommand(const Command &command, const QVariantMap &data);

private slots:
    void onTriggered()
    {
        Command command = m_command;
        if ( command.outputTab.isEmpty() )
            command.outputTab = m_browser->tabName();

        QVariantMap dataMap;

        if (m_type == ClipboardCommand) {
            const QMimeData *data = clipboardData();
            if (data == NULL)
                setTextData( &dataMap, m_browser->selectedText() );
            else
                dataMap = cloneData(*data);
        } else {
            dataMap = m_browser->getSelectedItemData();
        }

        emit triggerCommand(command, dataMap);
    }

private:
    Command m_command;
    Type m_type;
    ClipboardBrowser *m_browser;
};

} // namespace

class ScrollSaver {
public:
    ScrollSaver(QListView *view)
        : m_view(view)
        , m_index()
        , m_oldOffset(0)
        , m_currentSelected(false)
    {}

    void save()
    {
        m_index = m_view->currentIndex();
        m_oldOffset = m_view->visualRect(m_index).y();

        m_currentSelected = m_index.isValid() && m_oldOffset >= 0
                && m_oldOffset < m_view->viewport()->contentsRect().height();

        if (!m_currentSelected) {
            m_index = indexNear(m_view, 0);
            m_oldOffset = m_view->visualRect(m_index).y();
        }
    }

    void restore()
    {
        QModelIndex current = m_view->currentIndex();

        if ( !m_index.isValid() ) {
            if ( !current.isValid() )
                m_view->setCurrentIndex( m_view->model()->index(0, 0) );
            return;
        }

        if ( !current.isValid() || (m_currentSelected && m_index == current) ) {
            const int dy = m_view->visualRect(m_index).y() - m_oldOffset;
            if (dy != 0) {
                const int v = m_view->verticalScrollBar()->value();
                m_view->verticalScrollBar()->setValue(v + dy);
            }
        }
    }

private:
    QListView *m_view;
    QPersistentModelIndex m_index;
    int m_oldOffset;
    bool m_currentSelected;
};

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

ClipboardBrowser::ClipboardBrowser(QWidget *parent, const ClipboardBrowserSharedPtr &sharedData)
    : QListView(parent)
    , m_itemLoader()
    , m_tabName()
    , m_lastFiltered(-1)
    , m_update(false)
    , m( new ClipboardModel(this) )
    , d( new ItemDelegate(this) )
    , m_timerSave( new QTimer(this) )
    , m_timerScroll( new QTimer(this) )
    , m_timerUpdate( new QTimer(this) )
    , m_timerFilter( new QTimer(this) )
    , m_timerExpire(NULL)
    , m_menu(NULL)
    , m_invalidateCache(false)
    , m_expire(false)
    , m_editor(NULL)
    , m_sharedData(sharedData ? sharedData : ClipboardBrowserSharedPtr(new ClipboardBrowserShared))
    , m_loadButton(NULL)
    , m_searchProgress(NULL)
    , m_dragPosition(-1)
    , m_dragStartPosition()
    , m_spinLock(0)
    , m_updateLock(m_update)
    , m_scrollSaver()
{
    setLayoutMode(QListView::Batched);
    setBatchSize(1);
    setFrameShape(QFrame::NoFrame);
    setTabKeyNavigation(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setWrapping(false);
    setLayoutMode(QListView::SinglePass);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSpacing(5);
    setAlternatingRowColors(true);

    initSingleShotTimer(m_timerSave, 30000, SLOT(saveItems()));
    initSingleShotTimer(m_timerScroll, 50);
    initSingleShotTimer(m_timerUpdate, 10, SLOT(doUpdateCurrentPage()));
    initSingleShotTimer(m_timerFilter, 10, SLOT(filterItems()));

    // ScrollPerItem doesn't work well with hidden items
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    connect( verticalScrollBar(), SIGNAL(valueChanged(int)),
             SLOT(updateCurrentPage()) );

    setAttribute(Qt::WA_MacShowFocusRect, 0);

    setAcceptDrops(true);

    connectModelAndDelegate();
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

void ClipboardBrowser::onCommandActionTriggered(const Command &command, const QVariantMap &data)
{
    const QModelIndexList selected = selectedIndexes();

    if ( !command.cmd.isEmpty() ) {
        if (command.transform) {
            foreach (const QModelIndex &index, selected) {
                QVariantMap data = itemData( index.row() );
                if ( command.input.isEmpty() || hasFormat(data, command.input) )
                    emit requestActionDialog(data, command, index);
            }
        } else {
            emit requestActionDialog(data, command, QModelIndex());
        }
    }

    if ( !command.tab.isEmpty() && command.tab != tabName() ) {
        for (int i = selected.size() - 1; i >= 0; --i) {
            QVariantMap data = itemData( selected[i].row() );
            if ( !data.isEmpty() )
                emit addToTab(data, command.tab);
        }
    }

    if (command.remove) {
        const int lastRow = removeIndexes(selected);
        if (lastRow != -1)
            setCurrent(lastRow);
    }

    if (command.hideWindow)
        emit requestHide();
}

void ClipboardBrowser::createContextMenu()
{
    if (m_menu == NULL)
        return;

    foreach ( QAction *action, m_menu->actions() )
        delete action;

    updateContextMenu();
}

bool ClipboardBrowser::isFiltered(int row) const
{
    if ( d->searchExpression().isEmpty() || !m_itemLoader)
        return false;

    const QModelIndex ind = m->index(row);
    return !ConfigurationManager::instance()->itemFactory()->matches( ind, d->searchExpression() );
}

bool ClipboardBrowser::hideFiltered(int row)
{
    d->setRowVisible(row, false); // show in preload()

    bool hide = isFiltered(row);
    setRowHidden(row, hide);

    return hide;
}

bool ClipboardBrowser::startEditor(QObject *editor)
{
    connect( editor, SIGNAL(fileModified(QByteArray,QString)),
            this, SLOT(itemModified(QByteArray,QString)) );

    connect( editor, SIGNAL(closed(QObject *)),
             this, SLOT(closeExternalEditor(QObject *)) );

    connect( editor, SIGNAL(error(QString)),
             this, SIGNAL(error(QString)) );

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
    int offset = verticalOffset();

    // Find first index to preload.
    forever {
        ind = index(i);
        if ( !ind.isValid() )
            return;

        if ( !isIndexHidden(ind) ) {
            const int h = d->sizeHint(ind).height();
            y += h; // bottom of item
            if (y >= offset)
                break;
            y += s; // top of next item
        }

        d->setRowVisible(i, false);

        ++i;
    }

    // Absolute to relative.
    y -= offset;

    bool update = false;

    // Preload items backwards.
    forever {
        const int lastIndex = i;
        for ( ind = index(--i); ind.isValid() && isIndexHidden(ind); ind = index(--i) ) {}

        if ( !ind.isValid() ) {
            i = lastIndex;
            ind = index(i);
            break;
        }

        const QRect oldRect(visualRect(ind));

        // Fetch item.
        d->cache(ind);
        const int h = d->sizeHint(ind).height();

        // Re-layout rows afterwards if size has changed.
        const int dy = h - oldRect.height();
        if (dy != 0) {
            maxY += dy;
            update = true;
        }

        // Done?
        y -= h; // top of item
        if (y + h < minY)
            break;
        y -= s; // bottom of previous item
    }

    y = visualRect(ind).y();
    bool lastToPreload = false;

    // Render visible items, re-layout rows and correct scroll offset.
    forever {
        if (m_lastFiltered != -1 && m_lastFiltered < i)
            break;

        const QRect oldRect(update ? QRect() : visualRect(ind));

        // Fetch item.
        d->cache(ind);
        const int h = d->sizeHint(ind).height();

        // Re-layout rows afterwards if row position or size has changed.
        if (!update)
            update = (y != oldRect.y() || h != oldRect.height());

        // Correct widget position.
        d->updateRowPosition(i, y);

        d->setRowVisible(i, true);

        // Next.
        ind = index(++i);

        // Done?
        y += h + s; // top of item
        if (y > maxY) {
            if (lastToPreload)
                break;
            lastToPreload = true; // One more item to preload.
        }

        // Skip hidden.
        for ( ; ind.isValid() && isIndexHidden(ind); ind = index(++i) ) {}

        if ( !ind.isValid() )
            break;
    }

    // Hide the rest.
    for ( ; i < m->rowCount(); ++i )
        d->setRowVisible(i, false);

    if (update)
        scheduleDelayedItemsLayout();
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
            stopExpiring();
        } else {
            setFocus();
            if (isHidden())
                restartExpiring();
            if (m_invalidateCache)
                invalidateItemCache();
            if (m_expire)
                expire();
        }
    }

    setFocusProxy(m_editor);

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

    updateContextMenu();
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
    if ( m_itemLoader && m_timerExpire != NULL && isHidden() )
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
        if ( action->parent() == this )
            delete action;
        else if (m_menu != NULL)
            m_menu->removeAction(action);
    }

    // No actions should be left to remove.
    Q_ASSERT(m_menu == NULL || m_menu->isEmpty());
}

QAction *ClipboardBrowser::createAction(Actions::Id id, const char *slot)
{
    ConfigTabShortcuts *shortcuts = ConfigurationManager::instance()->tabShortcuts();
    QAction *act = shortcuts->action(id, this, Qt::WidgetShortcut);
    connect( act, SIGNAL(triggered()), this, slot, Qt::UniqueConnection );
    m_menu->addAction(act);
    return act;
}

QModelIndex ClipboardBrowser::indexNear(int offset) const
{
    return ::indexNear(this, offset);
}

void ClipboardBrowser::updateSearchProgress()
{
    if ( !d->searchExpression().isEmpty() && m_lastFiltered != -1 && m_lastFiltered < length() ) {
        if (m_searchProgress == NULL) {
            m_searchProgress = new QProgressBar(this);
        }
        m_searchProgress->setFormat( tr("Searching %p%...",
                                        "Text in progress bar for searching/filtering items; %p is amount in percent") );
        m_searchProgress->setRange(0, length());
        m_searchProgress->setValue(m_lastFiltered);
        const int margin = 8;
        m_searchProgress->setGeometry( margin, height() - m_searchProgress->height() - margin,
                                       viewport()->width() - 2 * margin, m_searchProgress->height() );
        m_searchProgress->show();
    } else {
        delete m_searchProgress;
        m_searchProgress = NULL;
    }
}

int ClipboardBrowser::getDropRow(const QPoint &position)
{
    const QModelIndex index = indexNear( position.y() );
    return index.isValid() ? index.row() : length();
}

void ClipboardBrowser::connectModelAndDelegate()
{
    Q_ASSERT(m != model());

    connect( m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(onDataChanged(QModelIndex,QModelIndex)) );
    connect( m, SIGNAL(tabNameChanged(QString)),
             SLOT(onTabNameChanged(QString)) );
    connect( m, SIGNAL(unloaded()),
             SLOT(onModelUnloaded()) );

    // update on change
    connect( m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(onModelDataChanged()) );
    connect( m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             SLOT(onModelDataChanged()) );
    connect( m, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
             SLOT(onModelDataChanged()) );
    connect( m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(onModelDataChanged()) );
    connect( d, SIGNAL(rowSizeChanged()),
             SLOT(updateCurrentPage()) );

    // delegate for rendering and editing items
    setItemDelegate(d);
    connect( m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             d, SLOT(rowsInserted(QModelIndex, int, int)) );
    connect( m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             d, SLOT(rowsRemoved(QModelIndex,int,int)) );
    connect( m, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
             d, SLOT(rowsMoved(QModelIndex, int, int, QModelIndex, int)) );
    connect( m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             d, SLOT(dataChanged(QModelIndex,QModelIndex)) );

    // set new model
    QAbstractItemModel *oldModel = model();
    setModel(m);
    delete oldModel;

    updateCurrentPage();
}

void ClipboardBrowser::updateItemMaximumSize()
{
    QSize maxSize(2048, 2048);

    if (m_sharedData->textWrap) {
        maxSize = viewport()->contentsRect().size();
        if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
             maxSize -= QSize(verticalScrollBar()->width(), 0);
    }

    d->setItemMaximumSize(maxSize);

    scheduleDelayedItemsLayout();
}

void ClipboardBrowser::addCommandsToMenu(QMenu *menu, const QVariantMap &data)
{
    if ( m_sharedData->commands.isEmpty() )
        return;

    CommandAction::Type type = (menu == m_menu) ? CommandAction::ItemCommand
                                                : CommandAction::ClipboardCommand;

    QList<QKeySequence> usedShortcuts;

    foreach (const Command &command, m_sharedData->commands) {
        // Verify that command can be added to menu.
        if ( !command.inMenu || command.name.isEmpty() )
            continue;

        if ( !canExecuteCommand(command, data, tabName()) )
            continue;

        QAction *act = new CommandAction(command, type, this);
        menu->addAction(act);

        if (type == CommandAction::ItemCommand) {
            QList<QKeySequence> uniqueShortcuts;

            foreach (const QString &shortcutText, command.shortcuts) {
                const QKeySequence shortcut(shortcutText, QKeySequence::PortableText);
                if ( !shortcut.isEmpty() && !usedShortcuts.contains(shortcut)  ) {
                    usedShortcuts.append(shortcut);
                    uniqueShortcuts.append(shortcut);
                }
            }

            act->setShortcuts(uniqueShortcuts);
        }

        connect(act, SIGNAL(triggerCommand(Command,QVariantMap)),
                this, SLOT(onCommandActionTriggered(Command,QVariantMap)));
    }

    if (type == CommandAction::ItemCommand)
        ConfigurationManager::instance()->tabShortcuts()->setDisabledShortcuts(usedShortcuts);
}

void ClipboardBrowser::lock()
{
    if (m_spinLock == 0) {
        m_scrollSaver.reset(new ScrollSaver(this));
        m_scrollSaver->save();

        bool update = autoUpdate();
        setAutoUpdate(false);
        m_updateLock = update;
        setUpdatesEnabled(false);
    }

    ++m_spinLock;
}

void ClipboardBrowser::unlock()
{
    Q_ASSERT(m_spinLock > 0);
    --m_spinLock;

    if (m_spinLock == 0) {
        setAutoUpdate(m_updateLock);

        m_scrollSaver->restore();
        m_scrollSaver.reset(NULL);

        setUpdatesEnabled(true);

        updateContextMenu();

        updateCurrentPage();
    }
}

bool ClipboardBrowser::hasUserSelection() const
{
    return isActiveWindow() || editing() || selectionModel()->selectedRows().count() > 1;
}

QVariantMap ClipboardBrowser::copyIndexes(const QModelIndexList &indexes, bool serializeItems) const
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    QByteArray text;
    QByteArray uriList;
    QVariantMap data;

    /* Copy items in reverse (items will be pasted correctly). */
    for ( int i = indexes.size()-1; i >= 0; --i ) {
        const QModelIndex ind = indexes.at(i);
        if ( isIndexHidden(ind) )
            continue;

        const ClipboardItemPtr &item = at( ind.row() );
        const QVariantMap copiedItemData = m_itemLoader ? m_itemLoader->copyItem(*m, item->data())
                                                        : item->data();

        if (serializeItems)
            stream << copiedItemData;

        if (indexes.size() == 1) {
            data = copiedItemData;
        } else {
            if ( !text.isEmpty() )
                text.prepend('\n');
            text.prepend( copiedItemData.value(mimeText).toByteArray() );

            QByteArray uri = copiedItemData.value(mimeUriList).toByteArray();
            if ( !uri.isEmpty() ) {
                if ( !uriList.isEmpty() )
                    uriList.prepend('\n');
                uriList.prepend(uri);
            }

            foreach ( const QString &format, copiedItemData.keys() ) {
                if ( !data.contains(format) )
                    data.insert(format, copiedItemData[format]);
            }
        }
    }

    if (serializeItems)
        data.insert(mimeItems, bytes);
    if (indexes.size()  > 1) {
        if ( !text.isNull() )
            data.insert(mimeText, text);
        if ( !uriList.isNull() )
            data.insert(mimeUriList, uriList);
    }

    return data;
}

int ClipboardBrowser::removeIndexes(const QModelIndexList &indexes)
{
    if ( indexes.isEmpty() )
        return -1;

    Q_ASSERT(m_itemLoader);
    m_itemLoader->itemsRemovedByUser(indexes);

    QList<int> rows;
    rows.reserve( indexes.size() );

    foreach (const QModelIndex &index, indexes) {
        if ( index.isValid() )
            rows.append( index.row() );
    }

    qSort( rows.begin(), rows.end(), qGreater<int>() );

    ClipboardBrowser::Lock lock(this);
    foreach (int row, rows) {
        if ( !isRowHidden(row) )
            m->removeRow(row);
    }

    return rows.last();
}

void ClipboardBrowser::paste(const QVariantMap &data, int destinationRow)
{
    ClipboardBrowser::Lock lock(this);

    int count = 0;

    // Insert items from clipboard or just clipboard content.
    if ( data.contains(mimeItems) ) {
        const QByteArray bytes = data[mimeItems].toByteArray();
        QDataStream stream(bytes);

        while ( !stream.atEnd() ) {
            QVariantMap dataMap;
            stream >> dataMap;
            add(dataMap, destinationRow);
            ++count;
        }
    } else {
        add(data, destinationRow);
        count = 1;
    }

    // Select new items.
    if (count > 0) {
        QItemSelection sel;
        QModelIndex first = index(destinationRow);
        QModelIndex last = index(destinationRow + count - 1);
        sel.select(first, last);
        setCurrentIndex(first);
        selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    }

    saveItems();
}

QPixmap ClipboardBrowser::renderItemPreview(const QModelIndexList &indexes, int maxWidth, int maxHeight)
{
    int h = 0;
    const int s = spacing();
    foreach (const QModelIndex &index, indexes) {
        if ( !isIndexHidden(index) )
            h += visualRect(index).height() + s;
    }

    if (h == 0)
        return QPixmap();

    const QRect rect(0, 0, qMin(maxWidth, viewport()->contentsRect().width()), qMin(maxHeight, h) );

    QPixmap pix( rect.size() );
    pix.fill(Qt::transparent);
    QPainter p(&pix);

    h = 0;
    const QPoint pos = viewport()->pos() - QPoint(s / 2 + 1, s / 2 + 1);
    foreach (const QModelIndex &index, indexes) {
        if ( isIndexHidden(index) )
            continue;
        render( &p, QPoint(0, h), visualRect(index).translated(pos).adjusted(0, 0, s, s) );
        h += visualRect(index).height() + s;
        if ( h > rect.height() )
            break;
    }

    p.setPen(Qt::black);
    p.setBrush(Qt::NoBrush);
    p.drawRect( 0, 0, rect.width() - 1, rect.height() - 1 );
    p.setPen(Qt::white);
    p.drawRect( 1, 1, rect.width() - 3, rect.height() - 3 );

    return pix;
}

void ClipboardBrowser::updateContextMenu()
{
    if (!m_menu || !m_itemLoader || !updatesEnabled())
        return;

    m_menu->clear();

    clearActions();

    if (editing())
        return;

    addCommandsToMenu(m_menu, getSelectedItemData());

    m_menu->addSeparator();

    initActions();

    emit contextMenuUpdated();
}

void ClipboardBrowser::onModelDataChanged()
{
    delayedSaveItems();
    updateCurrentPage();
}

void ClipboardBrowser::onDataChanged(const QModelIndex &a, const QModelIndex &b)
{
    if (editing()) {
        m_invalidateCache = true;
        return;
    }

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
}

void ClipboardBrowser::onTabNameChanged(const QString &tabName)
{
    if ( m_tabName.isEmpty() ) {
        m_tabName = tabName;
        return;
    }

    ConfigurationManager *cm = ConfigurationManager::instance();

    // Just move last saved file if tab is not loaded yet.
    if ( isLoaded() && cm->saveItemsWithOther(*m, &m_itemLoader) ) {
        m_timerSave->stop();
        cm->removeItems(m_tabName);
    } else {
        cm->moveItems(m_tabName, tabName);
    }

    m_tabName = tabName;
}

void ClipboardBrowser::updateCurrentPage()
{
    if ( !m_timerUpdate->isActive() )
        m_timerUpdate->start();
}

void ClipboardBrowser::doUpdateCurrentPage()
{
    if ( !updatesEnabled() ) {
        m_timerUpdate->start();
        return;
    }

    if ( !isLoaded() )
        return;
    if ( !isVisible() )
        return; // Update on showEvent().

    restartExpiring();

    const int h = viewport()->contentsRect().height();
    preload(-h, h);
    updateCurrentItem();
    m_timerUpdate->stop();
}

void ClipboardBrowser::expire()
{
    if (editing()) {
        m_expire = true;
    } else {
        m_expire = false;

        saveUnsavedItems();

        m->unloadItems();

        if ( isVisible() )
            loadItems();
    }
}

void ClipboardBrowser::onEditorDestroyed()
{
    setEditorWidget(NULL);
}

void ClipboardBrowser::onEditorSave()
{
    Q_ASSERT(m_editor != NULL);
    m_editor->commitData(m);
    saveItems();
}

void ClipboardBrowser::onEditorCancel()
{
    maybeCloseEditor();
}

void ClipboardBrowser::onModelUnloaded()
{
    m_itemLoader.clear();
}

void ClipboardBrowser::filterItems()
{
    m_timerFilter->stop();

    // row to select
    QModelIndex current = currentIndex();
    int first = current.isValid() && d->searchExpression().isEmpty() ? current.row() : -1;

    {
        ClipboardBrowser::Lock lock(this);

        // batch search
        QElapsedTimer t;
        t.start();

        for ( ++m_lastFiltered ; m_lastFiltered < length(); ++m_lastFiltered ) {
            if ( !hideFiltered(m_lastFiltered) && first == -1 )
                first = m_lastFiltered;
            if ( t.elapsed() > 25 ) {
                m_timerFilter->start();
                break;
            }
        }

        if ( m_lastFiltered < length() ) {
            // Hide the rest until found.
            for ( int row = m_lastFiltered + 1; row < length(); ++row ) {
                d->setRowVisible(row, false);
                setRowHidden(row, true);
            }
        } else {
            m_lastFiltered = -1;
        }
    }

    // select first visible
    if (!currentIndex().isValid() || sender() != m_timerFilter)
        setCurrentIndex( index(first) );

    updateSearchProgress();

    updateCurrentPage();
}

void ClipboardBrowser::contextMenuEvent(QContextMenuEvent *event)
{
    if ( m_menu == NULL || editing() || selectedIndexes().isEmpty() )
        return;

    QPoint pos = event->globalPos();

    // WORKAROUND: Fix menu position if text wrapping is disabled and
    //             item is too wide, event gives incorrect position on X axis.
    if (!m_sharedData->textWrap) {
        int menuMaxX = mapToGlobal( QPoint(width(), 0) ).x();
        if (pos.x() > menuMaxX)
            pos.setX(menuMaxX - width() / 2);
    }

    m_menu->exec(pos);
    event->accept();
}

void ClipboardBrowser::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);

    if (m_sharedData->textWrap)
        updateItemMaximumSize();

    if (m_loadButton != NULL)
        m_loadButton->resize( event->size() );

    updateSearchProgress();

    updateEditorGeometry();

    updateCurrentPage();
}

void ClipboardBrowser::showEvent(QShowEvent *event)
{
    stopExpiring();

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
    m_dragPosition = -1;
    update();
}

void ClipboardBrowser::dragMoveEvent(QDragMoveEvent *event)
{
    // Ignore unknown data from Qt widgets.
    const QStringList formats = event->mimeData()->formats();
    if ( formats.size() == 1 && formats[0].startsWith("application/x-q") )
        return;

    event->acceptProposedAction();
    m_dragPosition = event->pos().y();
    update();
}

void ClipboardBrowser::dropEvent(QDropEvent *event)
{
    event->accept();
    m_dragPosition = -1;
    if (event->dropAction() == Qt::MoveAction && event->source() == this)
        return; // handled in mouseMoveEvent()

    const QVariantMap data = cloneData( *event->mimeData() );
    paste( data, getDropRow(event->pos()) );
}

void ClipboardBrowser::paintEvent(QPaintEvent *e)
{
    QListView::paintEvent(e);

    // If dragging an item into list, draw indicator for dropping items.
    if (m_dragPosition >= 0) {
        const int s = spacing();

        QModelIndex pointedIndex = indexNear(m_dragPosition);

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

void ClipboardBrowser::mousePressEvent(QMouseEvent *event)
{
    if ( event->button() == Qt::LeftButton
         && (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) == 0
         && indexNear( event->pos().y() ).isValid() )
    {
        m_dragStartPosition = event->pos();
    }

    QListView::mousePressEvent(event);
}

void ClipboardBrowser::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragStartPosition = QPoint();
    QListView::mouseReleaseEvent(event);
}

void ClipboardBrowser::mouseMoveEvent(QMouseEvent *event)
{
    if ( m_dragStartPosition.isNull() ) {
        QListView::mouseMoveEvent(event);
        return;
    }

    if ( (event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance() )
         return;

    QModelIndex index = indexNear( m_dragStartPosition.y() );
    if ( !index.isValid() )
        return;

    QModelIndexList selected = selectedIndexes();
    if ( !selected.contains(index) ) {
        setCurrentIndex(index);
        selected.clear();
        selected.append(index);
    }

    qSort(selected);

    QVariantMap data = copyIndexes(selected);
    index = selected.first();

    QDrag *drag = new QDrag(this);
    drag->setMimeData( createMimeData(data) );
    drag->setPixmap( renderItemPreview(selected, 150, 150) );

    // Save persistent indexes so after the items are dropped (and added) these indexes remain valid.
    QList<QPersistentModelIndex> indexesToRemove;
    foreach (const QModelIndex &index, selected)
        indexesToRemove.append(index);

    // start dragging (doesn't block event loop)
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction);

    if (dropAction == Qt::MoveAction) {
        selected.clear();
        foreach (const QModelIndex &index, indexesToRemove)
            selected.append(index);
        QObject *target = drag->target();
        if (target == this || target == viewport()) {
            const int targetRow = getDropRow( mapFromGlobal(QCursor::pos()) );
            const QPersistentModelIndex targetIndex = this->index(targetRow);
            foreach (const QModelIndex &index, indexesToRemove) {
                const int from = index.row();
                const int to = targetIndex.row();
                m->move(from, from < to ? to - 1 : to);
            }
        } else if ( m_itemLoader->canMoveItems(selected) ) {
            removeIndexes(selected);
        }
    }

    update(); // Clear drag indicator.

    event->accept();
}

bool ClipboardBrowser::openEditor()
{
    const QModelIndexList selected = selectionModel()->selectedRows();
    return (selected.size() == 1) ? openEditor( selected.first() )
                                  : openEditor( selectedText().toUtf8() );
}

bool ClipboardBrowser::openEditor(const QByteArray &data, const QString &mime,
                                  const QString &editorCommand)
{
    if ( !isLoaded() )
        return false;

    const QString &cmd = editorCommand.isNull() ? m_sharedData->editor : editorCommand;
    if ( cmd.isEmpty() )
        return false;

    QObject *editor = new ItemEditor(data, mime, cmd, this);
    return startEditor(editor);
}

bool ClipboardBrowser::openEditor(const QModelIndex &index)
{
    if ( !isLoaded() )
        return false;

    ItemWidget *item = d->cache(index);
    QObject *editor = item->createExternalEditor(index, this);
    if (editor == NULL) {
        QVariantMap data = itemData( index.row() );
        if ( data.contains(mimeText) )
            editor = new ItemEditor(data[mimeText].toByteArray(), mimeText, m_sharedData->editor, this);
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
    const QModelIndex current = currentIndex();
    if ( current.isValid() ) {
        ClipboardItemPtr item = m->at( current.row() );
        QScopedPointer<ClipboardDialog> clipboardDialog(new ClipboardDialog(item, this));
        clipboardDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        clipboardDialog->show();
        clipboardDialog.take();
    }
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
    const QVariantMap data = getSelectedItemData();
    if ( !data.isEmpty() )
        emit requestActionDialog(data);
    else
        emit requestActionDialog( createDataMap(mimeText, selectedText()) );
}

void ClipboardBrowser::itemModified(const QByteArray &bytes, const QString &mime)
{
    // add new item
    if ( !bytes.isEmpty() ) {
        add( createDataMap(mime, bytes) );
        updateClipboard(0);
        saveItems();
    }
}

void ClipboardBrowser::filterItems(const QRegExp &re)
{
    // Do nothing if same regexp was already set or both are empty (don't compare regexp options).
    if ( (d->searchExpression().isEmpty() && re.isEmpty()) || d->searchExpression() == re )
        return;

    d->setSearch(re);

    // hide filtered items
    m_lastFiltered = -1;
    filterItems();
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
    if (m_sharedData->moveItemOnReturnKey && i != 0) {
        m->move(i,0);
        row = 0;
        scrollToTop();
    }
    if ( autoUpdate() )
        updateClipboard(row);
}

void ClipboardBrowser::editNew(const QString &text)
{
    if ( !isLoaded() )
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
            m->moveItemsWithKeyboard(selectedIndexes(), key);
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

                rect = visualRect(current);

                const int fromY = d > 0 ? qMax(0, rect.bottom()) : qMin(h, rect.y());
                const int y = fromY + d * h;
                QModelIndex ind = indexNear(y);
                if (!ind.isValid())
                    ind = index(d > 0 ? m->rowCount() - 1 : 0);

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

            setCurrent(row, false, mods.testFlag(Qt::ShiftModifier));
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

ClipboardItemPtr ClipboardBrowser::at(int row) const
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
    const QModelIndexList toRemove = selectedIndexes();
    Q_ASSERT(m_itemLoader);
    if ( !toRemove.isEmpty() && m_itemLoader->canRemoveItems(toRemove) ) {
        const int lastRow = removeIndexes(toRemove);
        if (lastRow != -1)
            setCurrent(lastRow);
    }
}

bool ClipboardBrowser::select(uint itemHash, bool moveToTop, bool moveToClipboard)
{
    int row = m->findItem(itemHash);
    if (row < 0)
        return false;

    if (moveToTop) {
        m->move(row, 0);
        row = 0;
        scrollToTop();
    }

    setCurrent(row);

    if (moveToClipboard)
        this->moveToClipboard(row);

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
    return add( createDataMap(mimeText, txt), row );
}

bool ClipboardBrowser::add(const QVariantMap &data, int row)
{
    bool keepUserSelection = hasUserSelection();

    QScopedPointer<ClipboardBrowser::Lock> lock;
    if ( updatesEnabled() && keepUserSelection )
        lock.reset(new ClipboardBrowser::Lock(this));

    if ( m->isDisabled() )
        return false;
    if ( !isLoaded() ) {
        loadItems();
        if ( !isLoaded() )
            return false;
    }

    // create new item
    int newRow = row < 0 ? m->rowCount() : qMin(row, m->rowCount());
    m->insertItem(data, newRow);

    // filter item
    if ( isFiltered(newRow) ) {
        setRowHidden(newRow, true);
    } else if (!keepUserSelection) {
        // Select new item if clipboard is not focused and the item is not filtered-out.
        clearSelection();
        setCurrent(newRow);
    }

    // list size limit
    if ( m->rowCount() > m_sharedData->maxItems )
        m->removeRow( m->rowCount() - 1 );

    delayedSaveItems();

    return true;
}

void ClipboardBrowser::loadSettings()
{
    ConfigurationManager *cm = ConfigurationManager::instance();

    expire();

    cm->tabAppearance()->decorateBrowser(this);

    // restore configuration
    m->setMaxItems(m_sharedData->maxItems);

    updateItemMaximumSize();

    d->setSaveOnEnterKey(m_sharedData->saveOnReturnKey);

    createContextMenu();

    updateCurrentPage();

    if (m_loadButton)
        updateLoadButtonIcon(m_loadButton);

    delete m_timerExpire;
    m_timerExpire = NULL;

    if (m_sharedData->minutesToExpire > 0) {
        m_timerExpire = new QTimer(this);
        initSingleShotTimer( m_timerExpire, 60000 * m_sharedData->minutesToExpire, SLOT(expire()) );
        restartExpiring();
    }

    if (m_editor) {
        d->loadEditorSettings(m_editor);
        setEditorWidget(m_editor);
    }
}

void ClipboardBrowser::loadItems()
{
    // Don't decrypt tab automatically if the operation was cancelled/unsuccessful previously.
    // In such case, decrypt only if unlock button was clicked.
    if ( m->isDisabled() && sender() != m_loadButton )
        return;

    loadItemsAgain();
}

void ClipboardBrowser::loadItemsAgain()
{
    restartExpiring();

    if ( isLoaded() )
        return;

    m_timerSave->stop();

    m->blockSignals(true);
    m_itemLoader = ConfigurationManager::instance()->loadItems(*m);
    m->blockSignals(false);

    // Show lock button if model is disabled.
    if ( !m->isDisabled() ) {
        delete m_loadButton;
        m_loadButton = NULL;
        d->rowsInserted(QModelIndex(), 0, m->rowCount());
        scheduleDelayedItemsLayout();
        updateCurrentPage();
        setCurrent(0);
    } else if (m_loadButton == NULL) {
        Q_ASSERT(length() == 0 && "Disabled model should be empty");
        m_loadButton = new QPushButton(this);
        m_loadButton->setFlat(true);
        m_loadButton->resize( size() );
        updateLoadButtonIcon(m_loadButton);
        m_loadButton->show();
        connect( m_loadButton, SIGNAL(clicked()),
                 this, SLOT(loadItems()) );
    }
}

bool ClipboardBrowser::saveItems()
{
    m_timerSave->stop();

    if ( !isLoaded() || tabName().isEmpty() )
        return false;

    ConfigurationManager::instance()->saveItems(*m, m_itemLoader);
    return true;
}

void ClipboardBrowser::delayedSaveItems()
{
    if ( !isLoaded() || tabName().isEmpty() || m_timerSave->isActive() )
        return;

    m_timerSave->start();
}

void ClipboardBrowser::saveUnsavedItems()
{
    if ( m_timerSave->isActive() )
        saveItems();
}

void ClipboardBrowser::purgeItems()
{
    if ( tabName().isEmpty() )
        return;
    ConfigurationManager::instance()->removeItems(tabName());
    m_timerSave->stop();
}

const QString ClipboardBrowser::selectedText() const
{
    QString result;

    foreach ( const QModelIndex &ind, selectionModel()->selectedIndexes() )
        result += itemText(ind) + QString('\n');
    result.chop(1);

    return result;
}

QString ClipboardBrowser::itemText(QModelIndex ind) const
{
    return ind.isValid() ? ind.data(Qt::EditRole).toString() : QString();
}

QVariantMap ClipboardBrowser::itemData(int i) const
{
    return m->dataMapInRow( i >= 0 ? i : currentIndex().row() );
}

void ClipboardBrowser::updateClipboard(int row)
{
    if ( row < m->rowCount() )
        emit changeClipboard( m->at(row)->data() );
}

QByteArray ClipboardBrowser::itemData(int i, const QString &mime) const
{
    const QVariantMap data = itemData(i);
    if ( data.isEmpty() )
        return QByteArray();

    return mime == "?" ? QStringList(data.keys()).join("\n").toUtf8() + '\n'
                       : data.value(mime).toByteArray();
}

void ClipboardBrowser::editRow(int row)
{
    editItem( index(row) );
}

void ClipboardBrowser::invalidateItemCache()
{
    if (editing()) {
        m_invalidateCache = true;
    } else {
        m_invalidateCache = false;
        d->invalidateCache();
        updateCurrentPage();
    }
}

void ClipboardBrowser::setAutoUpdate(bool update)
{
    m_updateLock = update;
    if (m_spinLock == 0)
        m_update = update;
}

void ClipboardBrowser::setContextMenu(QMenu *menu)
{
    clearActions();
    m_menu = menu;
    createContextMenu();
}

void ClipboardBrowser::setTabName(const QString &id)
{
    m->setTabName(id);
}

bool ClipboardBrowser::editing() const
{
    return m_editor != NULL;
}

bool ClipboardBrowser::isLoaded() const
{
    return ( m_itemLoader && !m->isDisabled() ) || tabName().isEmpty();
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
    case Qt::Key_F:
    case Qt::Key_D:
    case Qt::Key_B:
    case Qt::Key_U:
        if (mods & Qt::ControlModifier) {
            key = (key == Qt::Key_F || key == Qt::Key_D) ? Qt::Key_PageDown : Qt::Key_PageUp;
            mods = mods & ~Qt::ControlModifier;
        } else {
            return false;
        }
        break;
    default:
        return false;
    }

    QKeyEvent event2(QEvent::KeyPress, key, mods, event->text());
    keyPressEvent(&event2);
    event->accept();

    return true;
}

QVariantMap ClipboardBrowser::getSelectedItemData() const
{
    QModelIndexList selected = selectedIndexes();
    return copyIndexes(selected, false);
}

bool canExecuteCommand(const Command &command, const QVariantMap &data, const QString &sourceTabName)
{
    // Verify that an action is provided.
    if ( command.cmd.isEmpty() && !command.remove
         && (command.tab.isEmpty() || command.tab == sourceTabName) )
    {
        return false;
    }

    // Verify that data for given MIME is available.
    if ( !command.input.isEmpty() ) {
        const QList<QString> availableFormats = data.keys();
        if (command.input == mimeItems) {
            // Disallow applying action that takes serialized item more times.
            if ( availableFormats.contains(command.output) )
                return false;
        } else if ( !availableFormats.contains(command.input) ) {
            return false;
        }
    }

    // Verify that text is present when regex is defined.
    if ( !command.re.isEmpty() && !data.contains(mimeText) )
        return false;

    // Verify that and text, MIME type and window title are matched.
    const QString text = getTextData(data);
    const QString windowTitle = data.value(mimeWindowTitle).toString();
    if ( command.re.indexIn(text) == -1 || command.wndre.indexIn(windowTitle) == -1 )
        return false;

    // Verify that match command accepts item text.
    if ( !command.matchCmd.isEmpty() ) {
        Action matchAction(command.matchCmd, text.toUtf8(), QStringList(text), QStringList());
        matchAction.start();

        // TODO: This should be async, i.e. create object (in new thread) that validates command and
        //       emits a signal if successful.
        if ( !matchAction.waitForFinished(4000) ) {
            matchAction.terminate();
            if ( !matchAction.waitForFinished(1000) )
                matchAction.kill();
            return false;
        }

        if (matchAction.exitStatus() != QProcess::NormalExit || matchAction.exitCode() != 0)
            return false;
    }

    return true;
}

typedef QSharedPointer<ScrollSaver> ScrollSaverPtr;

#include "clipboardbrowser.moc"

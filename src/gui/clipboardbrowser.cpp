/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "common/appconfig.h"
#include "common/action.h"
#include "common/common.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "gui/clipboarddialog.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/theme.h"
#include "item/itemeditor.h"
#include "item/itemeditorwidget.h"
#include "item/itemfactory.h"
#include "item/itemstore.h"
#include "item/itemwidget.h"

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
#include <QElapsedTimer>

namespace {

bool alphaSort(const QModelIndex &lhs, const QModelIndex &rhs)
{
    const QString lhsText = lhs.data(contentType::text).toString();
    const QString rhsText = rhs.data(contentType::text).toString();
    return lhsText.localeAwareCompare(rhsText) < 0;
}

bool reverseSort(const QModelIndex &lhs, const QModelIndex &rhs)
{
    return lhs.row() > rhs.row();
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
    const QIcon icon( getIcon("", IconRepeat) );
    loadButton->setIconSize( QSize(64, 64) );
    loadButton->setIcon(icon);
}

void appendTextData(const QVariantMap &data, const QString &mime, QByteArray *lines)
{
    const QString text = getTextData(data, mime);

    if (text.isEmpty())
        return;

    if ( !lines->isEmpty() )
        lines->append('\n');
    lines->append(text.toUtf8());
}

} // namespace

QVariantMap itemData(const QModelIndex &index)
{
    return index.data(contentType::data).toMap();
}

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

ClipboardBrowserShared::ClipboardBrowserShared(ItemFactory *itemFactory)
    : editor()
    , maxItems(100)
    , textWrap(true)
    , viMode(false)
    , saveOnReturnKey(false)
    , moveItemOnReturnKey(false)
    , minutesToExpire(0)
    , itemFactory(itemFactory)
{
}

void ClipboardBrowserShared::loadFromConfiguration()
{
    AppConfig appConfig;
    editor = appConfig.option<Config::editor>();
    maxItems = appConfig.option<Config::maxitems>();
    textWrap = appConfig.option<Config::text_wrap>();
    viMode = appConfig.option<Config::vi>();
    saveOnReturnKey = !appConfig.option<Config::edit_ctrl_return>();
    moveItemOnReturnKey = appConfig.option<Config::move>();
    minutesToExpire = appConfig.option<Config::expire_tab>();
}

ClipboardBrowser::ClipboardBrowser(const ClipboardBrowserSharedPtr &sharedData, QWidget *parent)
    : QListView(parent)
    , m_itemLoader(NULL)
    , m_tabName()
    , m_lastFiltered(-1)
    , m(this)
    , d(this, sharedData->itemFactory)
    , m_invalidateCache(false)
    , m_expireAfterEditing(false)
    , m_editor(NULL)
    , m_sharedData(sharedData)
    , m_loadButton(NULL)
    , m_searchProgress(NULL)
    , m_dragTargetRow(-1)
    , m_dragStartPosition()
    , m_spinLock(0)
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
    setAlternatingRowColors(true);

    initSingleShotTimer( &m_timerSave, 30000, this, SLOT(saveItems()) );
    initSingleShotTimer( &m_timerScroll, 50 );
    initSingleShotTimer( &m_timerUpdate, 10, this, SLOT(doUpdateCurrentPage()) );
    initSingleShotTimer( &m_timerFilter, 10, this, SLOT(filterItems()) );
    initSingleShotTimer( &m_timerExpire, 0, this, SLOT(expire()) );

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
    d.invalidateCache();
    if ( m_timerSave.isActive() )
        saveItems();
}


void ClipboardBrowser::closeExternalEditor(QObject *editor)
{
    editor->disconnect(this);
    disconnect(editor);
    editor->deleteLater();
}

bool ClipboardBrowser::isFiltered(int row) const
{
    if ( d.searchExpression().isEmpty() || !m_itemLoader)
        return false;

    const QModelIndex ind = m.index(row);
    return m_sharedData->itemFactory && !m_sharedData->itemFactory->matches( ind, d.searchExpression() );
}

bool ClipboardBrowser::hideFiltered(int row)
{
    d.setRowVisible(row, false); // show in preload()

    bool hide = isFiltered(row);
    setRowHidden(row, hide);

    return hide;
}

bool ClipboardBrowser::startEditor(QObject *editor, bool changeClipboard)
{
    connect( editor, SIGNAL(fileModified(QByteArray,QString)),
             this, SLOT(itemModified(QByteArray,QString)) );

    if (changeClipboard) {
        connect( editor, SIGNAL(fileModified(QByteArray,QString)),
                 this, SLOT(onEditorNeedsChangeClipboard(QByteArray,QString)) );
    }

    connect( editor, SIGNAL(closed(QObject*)),
             this, SLOT(closeExternalEditor(QObject*)) );

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
            const int h = d.sizeHint(ind).height();
            y += h; // bottom of item
            if (y >= offset)
                break;
            y += s; // top of next item
        }

        d.setRowVisible(i, false);

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
        d.cache(ind);
        const int h = d.sizeHint(ind).height();

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
        d.cache(ind);
        const int h = d.sizeHint(ind).height();

        // Re-layout rows afterwards if row position or size has changed.
        if (!update)
            update = (y != oldRect.y() || h != oldRect.height());

        // Correct widget position.
        d.updateRowPosition(i, y);

        d.setRowVisible(i, true);

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
    for ( ; i < m.rowCount(); ++i )
        d.setRowVisible(i, false);

    if (update)
        scheduleDelayedItemsLayout();
}

void ClipboardBrowser::setEditorWidget(ItemEditorWidget *editor, bool changeClipboard)
{
    bool active = editor != NULL;

    if (m_editor != editor) {
        m_editor = editor;
        if (active) {
            connect( editor, SIGNAL(destroyed()),
                     this, SLOT(onEditorDestroyed()) );
            connect( editor, SIGNAL(save()),
                     this, SLOT(onEditorSave()) );
            if (changeClipboard) {
                connect( editor, SIGNAL(save()),
                         this, SLOT(onEditorNeedsChangeClipboard()) );
            }
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
            if (m_expireAfterEditing)
                expire();
        }
    }

    setFocusProxy(m_editor);

    setAcceptDrops(!active);

    // Hide scrollbars while editing.
    Qt::ScrollBarPolicy scrollbarPolicy = Qt::ScrollBarAlwaysOff;
    if (!active) {
        scrollbarPolicy = AppConfig(AppConfig::ThemeCategory).isOptionOn("show_scrollbars", true)
                ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    }
    setVerticalScrollBarPolicy(scrollbarPolicy);
    setHorizontalScrollBarPolicy(scrollbarPolicy);

    emit updateContextMenu();
}

void ClipboardBrowser::editItem(const QModelIndex &index, bool editNotes, bool changeClipboard)
{
    if (!index.isValid())
        return;

    ItemEditorWidget *editor = d.createCustomEditor(this, index, editNotes);
    if (editor != NULL) {
        if ( editor->isValid() )
            setEditorWidget(editor, changeClipboard);
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

bool ClipboardBrowser::canExpire()
{
    return m_itemLoader && m_timerExpire.interval() > 0 && !isVisible();
}

void ClipboardBrowser::restartExpiring()
{
    if (canExpire())
        m_timerExpire.start();
    else
        m_timerExpire.stop();
}

void ClipboardBrowser::stopExpiring()
{
    m_timerExpire.stop();
}

void ClipboardBrowser::updateCurrentItem()
{
    const QModelIndex current = currentIndex();
    if ( current.isValid() && d.hasCache(current) ) {
        ItemWidget *item = d.cache(current);
        item->setCurrent(false);
        item->setCurrent( hasFocus() );
    }
}

QModelIndex ClipboardBrowser::indexNear(int offset) const
{
    return ::indexNear(this, offset);
}

void ClipboardBrowser::updateSearchProgress()
{
    if ( !d.searchExpression().isEmpty() && m_lastFiltered != -1 && m_lastFiltered < length() ) {
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
    Q_ASSERT(&m != model());

    // set new model
    QAbstractItemModel *oldModel = model();
    setModel(&m);
    delete oldModel;

    // delegate for rendering and editing items
    setItemDelegate(&d);

    connect( &m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(onDataChanged(QModelIndex,QModelIndex)) );
    connect( &m, SIGNAL(tabNameChanged(QString)),
             SLOT(onTabNameChanged(QString)) );
    connect( &m, SIGNAL(unloaded()),
             SLOT(onModelUnloaded()) );

    // update on change
    connect( &m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(onModelDataChanged()) );
    connect( &m, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
             SLOT(onModelDataChanged()) );
    connect( &m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(onItemCountChanged()) );
    connect( &m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             SLOT(onItemCountChanged()) );
    connect( &m, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)),
             SLOT(onModelDataChanged()) );
    connect( &m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(onModelDataChanged()) );
    connect( &d, SIGNAL(rowSizeChanged()),
             SLOT(updateCurrentPage()) );

    connect( &m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             &d, SLOT(rowsInserted(QModelIndex, int, int)) );
    connect( &m, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
             &d, SLOT(rowsRemoved(QModelIndex,int,int)) );
    connect( &m, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)),
             &d, SLOT(rowsMoved(QModelIndex, int, int, QModelIndex, int)) );
    connect( &m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             &d, SLOT(dataChanged(QModelIndex,QModelIndex)) );

    updateCurrentPage();
}

void ClipboardBrowser::updateItemMaximumSize()
{
    QSize minSize = viewport()->contentsRect().size();
    if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
         minSize -= QSize(verticalScrollBar()->width(), 0);
    d.setItemSizes(m_sharedData->textWrap ? minSize : QSize(2048, 2048), minSize.width());

    scheduleDelayedItemsLayout();
}

void ClipboardBrowser::lock()
{
    ++m_spinLock;

    if (m_spinLock == 1) {
        m_scrollSaver.reset(new ScrollSaver(this));
        m_scrollSaver->save();
        setUpdatesEnabled(false);
    }
}

void ClipboardBrowser::unlock()
{
    Q_ASSERT(m_spinLock > 0);

    if (m_spinLock == 1) {
        setUpdatesEnabled(true);
        updateCurrentPage();

        m_scrollSaver->restore();
        if (m_spinLock == 1)
            m_scrollSaver.reset(NULL);
    }

    --m_spinLock;
}

void ClipboardBrowser::refilterItems()
{
    if ( !isLoaded() )
        return;

    bool showAll = d.searchExpression().isEmpty();

    // Hide the rest until found.
    for ( int row = 0; row < length(); ++row ) {
        d.setRowVisible(row, showAll);
        setRowHidden(row, !showAll);
    }

    m_lastFiltered = -1;
    filterItems();

    // Select row by number specified in search.
    bool rowSpecified;
    int selectedRow = d.searchExpression().pattern().toInt(&rowSpecified);
    if (rowSpecified && selectedRow >= 0 && selectedRow < length()) {
        d.setRowVisible(selectedRow, false); // Show in preload().
        setRowHidden(selectedRow, false);
        setCurrentIndex( index(selectedRow) );
    }

    scrollTo(currentIndex());
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

    for ( int i = 0; i < indexes.size(); ++i ) {
        const QModelIndex ind = indexes.at(i);
        if ( isIndexHidden(ind) )
            continue;

        const QVariantMap copiedItemData =
                m_itemLoader ? m_itemLoader->copyItem(m, itemData(ind)) : itemData(ind);

        if (serializeItems)
            stream << copiedItemData;

        if (indexes.size() == 1) {
            data = copiedItemData;
        } else {
            appendTextData(copiedItemData, mimeText, &text);
            appendTextData(copiedItemData, mimeUriList, &uriList);
        }
    }

    if (serializeItems)
        data.insert(mimeItems, bytes);

    if (indexes.size() > 1) {
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
            m.removeRow(row);
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
            add(dataMap, destinationRow + count);
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
        emit updateContextMenu();
}

void ClipboardBrowser::onItemCountChanged()
{
    if (isLoaded())
        emit itemCountChanged( tabName(), length() );
}

void ClipboardBrowser::onTabNameChanged(const QString &tabName)
{
    if ( m_tabName.isEmpty() ) {
        m_tabName = tabName;
        return;
    }

    // Just move last saved file if tab is not loaded yet.
    if ( isLoaded() && saveItemsWithOther(m, m_itemLoader, m_sharedData->itemFactory) ) {
        m_timerSave.stop();
        removeItems(m_tabName);
    } else {
        moveItems(m_tabName, tabName);
    }

    m_tabName = tabName;
}

void ClipboardBrowser::updateCurrentPage()
{
    if ( !m_timerUpdate.isActive() )
        m_timerUpdate.start();
}

void ClipboardBrowser::doUpdateCurrentPage()
{
    if ( !updatesEnabled() ) {
        m_timerUpdate.start();
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
    m_timerUpdate.stop();
}

void ClipboardBrowser::expire()
{
    if (editing()) {
        m_expireAfterEditing = true;
    } else {
        m_expireAfterEditing = false;

        saveUnsavedItems();

        m.unloadItems();

        if ( isVisible() )
            loadItems();
    }
}

void ClipboardBrowser::onEditorDestroyed()
{
    setEditorWidget(NULL);
    // Set the focus back on the browser
    setFocus();
}

void ClipboardBrowser::onEditorSave()
{
    Q_ASSERT(m_editor != NULL);
    m_editor->commitData(&m);
    saveItems();
}

void ClipboardBrowser::onEditorCancel()
{
    maybeCloseEditor();
}

void ClipboardBrowser::onModelUnloaded()
{
    m_itemLoader = NULL;
}

void ClipboardBrowser::onEditorNeedsChangeClipboard()
{
    QModelIndex index = m_editor->index();
    if (index.isValid())
        emit changeClipboard(itemData(index));
}

void ClipboardBrowser::onEditorNeedsChangeClipboard(const QByteArray &bytes, const QString &mime)
{
    emit changeClipboard(createDataMap(mime, bytes));
}

void ClipboardBrowser::filterItems()
{
    m_timerFilter.stop();

    if ( d.searchExpression().isEmpty() )
        return;

    // row to select
    QModelIndex current = currentIndex();
    int first = current.isValid() && d.searchExpression().isEmpty() ? current.row() : -1;

    {
        ClipboardBrowser::Lock lock(this);

        // batch search
        QElapsedTimer t;
        t.start();

        for ( ++m_lastFiltered ; m_lastFiltered < length(); ++m_lastFiltered ) {
            if ( isRowHidden(m_lastFiltered) && !hideFiltered(m_lastFiltered) && first == -1 )
                first = m_lastFiltered;

            if ( t.elapsed() > 25 ) {
                m_timerFilter.start();
                break;
            }
        }

        if ( m_lastFiltered >= length() )
            m_lastFiltered = -1;
    }

    // Select row specified by search or first visible.
    if (!currentIndex().isValid() || sender() != &m_timerFilter)
        setCurrentIndex( index(first) );

    updateSearchProgress();

    updateCurrentPage();
}

void ClipboardBrowser::contextMenuEvent(QContextMenuEvent *event)
{
    if ( editing() || selectedIndexes().isEmpty() )
        return;

    QPoint pos = event->globalPos();

    // WORKAROUND: Fix menu position if text wrapping is disabled and
    //             item is too wide, event gives incorrect position on X axis.
    if (!m_sharedData->textWrap) {
        int menuMaxX = mapToGlobal( QPoint(width(), 0) ).x();
        if (pos.x() > menuMaxX)
            pos.setX(menuMaxX - width() / 2);
    }

    emit showContextMenu(pos);
}

void ClipboardBrowser::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);

    Lock updateGeometryLock(this);

    updateItemMaximumSize();

    if (m_loadButton != NULL)
        m_loadButton->resize( event->size() );

    updateSearchProgress();

    updateEditorGeometry();
}

void ClipboardBrowser::showEvent(QShowEvent *event)
{
    stopExpiring();

    loadItems();

    if (!currentIndex().isValid())
        setCurrent(0);
    if ( m.rowCount() > 0 && !d.hasCache(index(0)) )
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

    if ( previous.isValid() && d.hasCache(previous) ) {
        ItemWidget *item = d.cache(previous);
        item->setCurrent(false);
    }

    updateCurrentItem();
}

void ClipboardBrowser::selectionChanged(const QItemSelection &selected,
                                        const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    emit updateContextMenu();
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
    m_dragTargetRow = -1;
    update();
}

void ClipboardBrowser::dragMoveEvent(QDragMoveEvent *event)
{
    // Ignore unknown data from Qt widgets.
    const QStringList formats = event->mimeData()->formats();
    if ( formats.size() == 1 && formats[0].startsWith("application/x-q") )
        return;

    event->acceptProposedAction();
    m_dragTargetRow = getDropRow(event->pos());
    update();
}

void ClipboardBrowser::dropEvent(QDropEvent *event)
{
    event->accept();
    if (event->dropAction() == Qt::MoveAction && event->source() == this)
        return; // handled in mouseMoveEvent()

    const QVariantMap data = cloneData( *event->mimeData() );
    paste( data, getDropRow(event->pos()) );
    m_dragTargetRow = -1;
}

void ClipboardBrowser::paintEvent(QPaintEvent *e)
{
    QListView::paintEvent(e);

    // If dragging an item into list, draw indicator for dropping items.
    if (m_dragTargetRow != -1) {
        const int s = spacing();

        QModelIndex pointedIndex = index(m_dragTargetRow);

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

        QWidget *target = qobject_cast<QWidget*>(drag->target());

        // Move items only if target is this app.
        if (target == this || target == viewport()) {
            foreach (const QModelIndex &index, indexesToRemove) {
                const int sourceRow = index.row();
                const int targetRow =
                        sourceRow < m_dragTargetRow ? m_dragTargetRow - 1 : m_dragTargetRow;
                m.move(sourceRow, targetRow);
            }
        } else if ( target && target->window() == window()
                    && m_itemLoader->canMoveItems(selected) )
        {
            removeIndexes(selected);
        }
    }

    // Clear drag indicator.
    m_dragTargetRow = -1;
    update();

    event->accept();
}

bool ClipboardBrowser::openEditor()
{
    const QModelIndexList selected = selectionModel()->selectedRows();
    return (selected.size() == 1) ? openEditor( selected.first() )
                                  : openEditor( selectedText().toUtf8() );
}

bool ClipboardBrowser::openEditor(const QByteArray &textData, bool changeClipboard)
{
    if ( !isLoaded() )
        return false;

    if ( m_sharedData->editor.isEmpty() )
        return false;

    QObject *editor = new ItemEditor(textData, mimeText, m_sharedData->editor, this);
    return startEditor(editor, changeClipboard);
}

bool ClipboardBrowser::openEditor(const QModelIndex &index)
{
    if ( !isLoaded() )
        return false;

    ItemWidget *item = d.cache(index);
    QObject *editor = item->createExternalEditor(index, this);
    if (editor == NULL) {
        const QVariantMap data = itemData(index);
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
        QScopedPointer<ClipboardDialog> clipboardDialog(
                    new ClipboardDialog(currentIndex(), &m, this) );
        clipboardDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        clipboardDialog->show();
        clipboardDialog.take();
    }
}

void ClipboardBrowser::removeRow(int row)
{
    const QModelIndex indexToRemove = index(row);
    if ( !indexToRemove.isValid() )
        return;

    bool removingCurrent = indexToRemove == currentIndex();

    Q_ASSERT(m_itemLoader);
    m_itemLoader->itemsRemovedByUser(QList<QModelIndex>() << indexToRemove);
    m.removeRow(row);

    if (removingCurrent)
        setCurrentIndex( index(qMin(row, length() - 1)) );
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

void ClipboardBrowser::itemModified(const QByteArray &bytes, const QString &mime)
{
    // add new item
    if ( !bytes.isEmpty() ) {
        add( createDataMap(mime, bytes) );
        saveItems();
    }
}

void ClipboardBrowser::filterItems(const QRegExp &re)
{
    // Do nothing if same regexp was already set or both are empty (don't compare regexp options).
    if ( (d.searchExpression().isEmpty() && re.isEmpty()) || d.searchExpression() == re )
        return;

    d.setSearch(re);

    refilterItems();
}

void ClipboardBrowser::moveToClipboard(const QModelIndex &ind)
{
    if ( !ind.isValid() )
        return;

    QPersistentModelIndex index = ind;

    if (m_sharedData->moveItemOnReturnKey && index.row() != 0) {
        m.move(index.row(), 0);
        scrollToTop();
    }

    emit changeClipboard( itemData(index) );
}

void ClipboardBrowser::editNew(const QString &text, bool changeClipboard)
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
    editItem( index(0), false, changeClipboard );
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    // ignore any input if editing an item
    if ( editing() )
        return;

    // translate keys for vi mode
    if (m_sharedData->viMode && handleViKey(event, this))
        return;

    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();

    if (mods != Qt::AltModifier) {
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
                if (m_timerScroll.isActive())
                    break;
                m_timerScroll.start();

                d = (key == Qt::Key_PageDown) ? 1 : -1;

                QRect rect = visualRect(current);

                if ( rect.height() > h && d < 0 ? rect.top() < 0 : rect.bottom() > h ) {
                    // Scroll within long item.
                    QScrollBar *v = verticalScrollBar();
                    v->setValue( v->value() + d * v->pageStep() );
                    break;
                }

                if ( row == (d > 0 ? m.rowCount() - 1 : 0) )
                    break; // Nothing to do.

                const int minY = d > 0 ? 0 : -h;
                preload(minY, minY + 2 * h);

                rect = visualRect(current);

                const int fromY = d > 0 ? qMax(0, rect.bottom()) : qMin(h, rect.y());
                const int y = fromY + d * h;
                QModelIndex ind = indexNear(y);
                if (!ind.isValid())
                    ind = index(d > 0 ? m.rowCount() - 1 : 0);

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

            const QItemSelectionModel::SelectionFlags flags = selectionCommand(index(row), event);
            const bool setCurrentOnly = flags.testFlag(QItemSelectionModel::NoUpdate);
            const bool keepSelection = setCurrentOnly || flags.testFlag(QItemSelectionModel::SelectCurrent);
            setCurrent(row, false, keepSelection, setCurrentOnly);
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

void ClipboardBrowser::setCurrent(int row, bool cycle, bool keepSelection, bool setCurrentOnly)
{
    QModelIndex prev = currentIndex();
    int cur = prev.row();

    // direction
    int dir = cur <= row ? 1 : -1;

    // select first visible
    int i = m.getRowNumber(row, cycle);
    cur = i;
    while ( isRowHidden(i) ) {
        i = m.getRowNumber(i+dir, cycle);
        if ( (!cycle && (i==0 || i==m.rowCount()-1)) || i == cur)
            break;
    }
    if ( isRowHidden(i) )
        return;

    QModelIndex ind = index(i);
    if (keepSelection) {
        ClipboardBrowser::Lock lock(this);
        QItemSelectionModel *sel = selectionModel();
        const bool currentSelected = sel->isSelected(prev);
        for ( int j = prev.row(); j != i + dir; j += dir ) {
            QModelIndex ind = index(j);
            if ( !ind.isValid() )
                break;
            if ( isIndexHidden(ind) )
                continue;

            if (!setCurrentOnly) {
                if ( sel->isSelected(ind) && sel->isSelected(prev) )
                    sel->setCurrentIndex(currentIndex(), QItemSelectionModel::Deselect);
                sel->setCurrentIndex(ind, QItemSelectionModel::Select);
            }
            prev = ind;
        }

        if (setCurrentOnly)
            sel->setCurrentIndex(prev, QItemSelectionModel::NoUpdate);
        else if (!currentSelected)
            sel->setCurrentIndex(prev, QItemSelectionModel::Deselect);
    } else {
        clearSelection();
        if (setCurrentOnly)
            selectionModel()->setCurrentIndex(ind, QItemSelectionModel::NoUpdate);
        else
            setCurrentIndex(ind);
    }
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

bool ClipboardBrowser::select(uint itemHash, SelectActions selectActions)
{
    int row = m.findItem(itemHash);
    if (row < 0)
        return false;

    if (selectActions.testFlag(MoveToTop)) {
        m.move(row, 0);
        row = 0;
        scrollToTop();
    }

    setCurrent(row);

    if (selectActions.testFlag(MoveToClipboard))
        moveToClipboard( index(row) );

    return true;
}

void ClipboardBrowser::sortItems(const QModelIndexList &indexes)
{
    m.sortItems(indexes, &alphaSort);
}

void ClipboardBrowser::reverseItems(const QModelIndexList &indexes)
{
    m.sortItems(indexes, &reverseSort);
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

    if ( m.isDisabled() )
        return false;
    if ( !isLoaded() ) {
        loadItems();
        if ( !isLoaded() )
            return false;
    }

    // create new item
    int newRow = row < 0 ? m.rowCount() : qMin(row, m.rowCount());
    m.insertItem(data, newRow);

    // filter item
    if ( isFiltered(newRow) ) {
        setRowHidden(newRow, true);
    } else if (!keepUserSelection) {
        // Select new item if clipboard is not focused and the item is not filtered-out.
        clearSelection();
        setCurrent(newRow);
    }

    // list size limit
    if ( m.rowCount() > m_sharedData->maxItems )
        m.removeRow( m.rowCount() - 1 );

    delayedSaveItems();

    return true;
}

void ClipboardBrowser::addUnique(const QVariantMap &data)
{
    if ( select(hash(data), MoveToTop) ) {
        COPYQ_LOG("New item: Moving existing to top");
        return;
    }

    QVariantMap newData = data;

    // Don't store internal formats.
    newData.remove(mimeWindowTitle);
    newData.remove(mimeOwner);
    newData.remove(mimeClipboardMode);
    newData.remove(mimeCurrentTab);
    newData.remove(mimeSelectedItems);
    newData.remove(mimeCurrentItem);
    newData.remove(mimeHidden);
    newData.remove(mimeShortcut);

#ifdef COPYQ_WS_X11
    // When selecting text under X11, clipboard data may change whenever selection changes.
    // Instead of adding item for each selection change, this updates previously added item.
    if ( !isClipboardData(data)
         && newData.contains(mimeText)
         // Don't update edited item.
         && (!editing() || currentIndex().row() != 0)
         )
    {
        const QModelIndex firstIndex = model()->index(0, 0);
        const QVariantMap previousData = itemData(firstIndex);

        if ( previousData.contains(mimeText)
             && getTextData(newData).contains(getTextData(previousData))
             )
        {
            COPYQ_LOG("New item: Merging with top item");

            const QSet<QString> formatsToAdd = previousData.keys().toSet() - newData.keys().toSet();

            foreach (const QString &format, formatsToAdd)
                newData.insert(format, previousData[format]);

            if ( add(newData) ) {
                const bool reselectFirst = !editing() && currentIndex().row() == 1;
                model()->removeRow(1);

                if (reselectFirst)
                    setCurrent(0);
            }

            return;
        }
    }
#endif

    COPYQ_LOG("New item: Adding");

    add(newData);
}

void ClipboardBrowser::loadSettings()
{
    expire();

    decorate( Theme() );

    // restore configuration
    m.setMaxItems(m_sharedData->maxItems);

    updateItemMaximumSize();

    d.setSaveOnEnterKey(m_sharedData->saveOnReturnKey);

    updateCurrentPage();

    if (m_loadButton)
        updateLoadButtonIcon(m_loadButton);

    m_timerExpire.setInterval(60000 * m_sharedData->minutesToExpire);

    restartExpiring();

    if (m_editor) {
        d.loadEditorSettings(m_editor);
        setEditorWidget(m_editor);
    }
}

void ClipboardBrowser::loadItems()
{
    // Don't decrypt tab automatically if the operation was cancelled/unsuccessful previously.
    // In such case, decrypt only if unlock button was clicked.
    if ( m.isDisabled() && sender() != m_loadButton )
        return;

    loadItemsAgain();
}

void ClipboardBrowser::loadItemsAgain()
{
    restartExpiring();

    if ( isLoaded() )
        return;

    m_timerSave.stop();

    m.blockSignals(true);
    m_itemLoader = ::loadItems(m, m_sharedData->itemFactory);
    m.blockSignals(false);

    // Show lock button if model is disabled.
    if ( !m.isDisabled() ) {
        delete m_loadButton;
        m_loadButton = NULL;
        d.rowsInserted(QModelIndex(), 0, m.rowCount());
        if ( !d.searchExpression().isEmpty() )
            refilterItems();
        scheduleDelayedItemsLayout();
        updateCurrentPage();
        setCurrent(0);
        onItemCountChanged();
        emit updateContextMenu();
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
    m_timerSave.stop();

    if ( !isLoaded() || tabName().isEmpty() )
        return false;

    ::saveItems(m, m_itemLoader);
    return true;
}

void ClipboardBrowser::moveToClipboard()
{
    moveToClipboard(currentIndex());
}

void ClipboardBrowser::delayedSaveItems()
{
    if ( !isLoaded() || tabName().isEmpty() || m_timerSave.isActive() )
        return;

    m_timerSave.start();
}

void ClipboardBrowser::saveUnsavedItems()
{
    if ( m_timerSave.isActive() )
        saveItems();
}

void ClipboardBrowser::purgeItems()
{
    if ( tabName().isEmpty() )
        return;

    removeItems(tabName());
    m_timerSave.stop();
}

const QString ClipboardBrowser::selectedText() const
{
    QString result;

    foreach ( const QModelIndex &ind, selectionModel()->selectedIndexes() )
        result += ind.data(Qt::EditRole).toString() + QString('\n');
    result.chop(1);

    return result;
}

void ClipboardBrowser::editRow(int row)
{
    editItem( index(row) );
}

void ClipboardBrowser::otherItemLoader(bool next)
{
    QModelIndex index = currentIndex();
    if ( index.isValid() )
        d.otherItemLoader(index, next);
}

void ClipboardBrowser::move(int key)
{
    m.moveItemsWithKeyboard(selectedIndexes(), key);
    scrollTo( currentIndex() );
}

void ClipboardBrowser::decorate(const Theme &theme)
{
    theme.decorateBrowser(this, &d);
    invalidateItemCache();
}

void ClipboardBrowser::invalidateItemCache()
{
    if (editing()) {
        m_invalidateCache = true;
    } else {
        m_invalidateCache = false;
        d.invalidateCache();
        updateCurrentPage();
    }
}

void ClipboardBrowser::setTabName(const QString &id)
{
    m.setTabName(id);
}

bool ClipboardBrowser::editing() const
{
    return m_editor != NULL;
}

bool ClipboardBrowser::isLoaded() const
{
    return !m_sharedData->itemFactory || ( m_itemLoader && !m.isDisabled() ) || tabName().isEmpty();
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

QVariantMap ClipboardBrowser::getSelectedItemData() const
{
    QModelIndexList selected = selectedIndexes();
    return copyIndexes(selected, false);
}

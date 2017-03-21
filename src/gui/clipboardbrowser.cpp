/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#include "common/textdata.h"
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
#include <QTemporaryFile>
#include <QUrl>
#include <QElapsedTimer>

#include <algorithm>
#include <functional>
#include <memory>

namespace {

/// Save drag'n'drop image data in temporary file (required by some applications).
class TemporaryDragAndDropImage : public QObject {
public:
    /// Return temporary image file for data or nullptr if file cannot be created.
    static TemporaryDragAndDropImage *create(QMimeData *mimeData, QObject *parent)
    {
        if ( !mimeData->hasImage() || mimeData->hasFormat(mimeUriList) )
            return nullptr;

        return new TemporaryDragAndDropImage(mimeData, parent);
    }

    /// Remove temporary image file after an interval (allows target application to read all data).
    void drop()
    {
        m_timerRemove.start();
    }

    ~TemporaryDragAndDropImage()
    {
        if ( m_filePath.isEmpty() )
            QFile::remove(m_filePath);
    }

private:
    TemporaryDragAndDropImage(QMimeData *mimeData, QObject *parent)
        : QObject(parent)
    {
        // Save image as PNG.
        const QImage image = mimeData->imageData().value<QImage>();

        QTemporaryFile imageFile;
        openTemporaryFile(&imageFile, ".png");
        image.save(&imageFile, "PNG");

        m_filePath = imageFile.fileName();
        imageFile.setAutoRemove(false);
        imageFile.close();

        // Add URI to temporary file to drag'n'drop data.
        const QUrl url = QUrl::fromLocalFile(m_filePath);
        const QByteArray localUrl = url.toString().toUtf8();
        mimeData->setData( mimeUriList, localUrl );

        // Set interval to wait before removing temporary file after data were dropped.
        const int transferRateBytesPerSecond = 100000;
        const int removeAfterDropSeconds = 5 + static_cast<int>(imageFile.size()) / transferRateBytesPerSecond;
        initSingleShotTimer( &m_timerRemove, removeAfterDropSeconds * 1000, this, SLOT(deleteLater()) );
    }

    QTimer m_timerRemove;
    QString m_filePath;
};

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

ClipboardBrowser::ClipboardBrowser(
        const QString &tabName,
        const ClipboardBrowserSharedPtr &sharedData,
        QWidget *parent)
    : QListView(parent)
    , m_itemSaver(nullptr)
    , m_tabName(tabName)
    , m(this)
    , d(this, sharedData)
    , m_editor(nullptr)
    , m_sharedData(sharedData)
    , m_dragTargetRow(-1)
    , m_dragStartPosition()
{
    setObjectName("ClipboardBrowser");

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
    initSingleShotTimer( &m_timerEmitItemCount, 0, this, SLOT(emitItemCount()) );

    // ScrollPerItem doesn't work well with hidden items
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    setAttribute(Qt::WA_MacShowFocusRect, false);

    setAcceptDrops(true);

    connectModelAndDelegate();

    m_sharedData->theme.decorateBrowser(this);
    updateItemMaximumSize();

    d.setSaveOnEnterKey(m_sharedData->saveOnReturnKey);
}

ClipboardBrowser::~ClipboardBrowser()
{
    d.invalidateCache();
    saveUnsavedItems();
}

bool ClipboardBrowser::moveToClipboard(uint itemHash)
{
    const int row = m.findItem(itemHash);
    if (row < 0)
        return false;

    setCurrent(row);
    moveToClipboard( index(row) );
    return true;
}

bool ClipboardBrowser::moveToTop(uint itemHash)
{
    const int row = m.findItem(itemHash);
    if (row < 0)
        return false;

    moveToTop( index(row) );
    return true;
}

void ClipboardBrowser::closeExternalEditor(QObject *editor)
{
    editor->disconnect(this);
    disconnect(editor);
    editor->deleteLater();
}

void ClipboardBrowser::emitItemCount()
{
    if (isLoaded())
        emit itemCountChanged( tabName(), length() );
}

bool ClipboardBrowser::eventFilter(QObject *, QEvent *event)
{
    // WORKAROUND: Update drag'n'drop when modifiers are pressed/released (QTBUG-57168).
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        const auto kev = static_cast<QKeyEvent*>(event);
        const auto key = kev->key();
        if (key == Qt::Key_Control || key == Qt::Key_Shift) {
            const auto screenPos = QCursor::pos();
            const auto localPos = mapFromGlobal(screenPos);
            QMouseEvent mouseMove(
                        QEvent::MouseMove, localPos, screenPos, Qt::NoButton,
                        QApplication::mouseButtons(), QApplication::queryKeyboardModifiers() );
            QCoreApplication::sendEvent(this, &mouseMove);
        }
    }

    return false;
}

bool ClipboardBrowser::isFiltered(int row) const
{
    if ( d.searchExpression().isEmpty() || !m_itemSaver)
        return false;

    const QModelIndex ind = m.index(row);
    return m_filterRow != row
            && m_sharedData->itemFactory
            && !m_sharedData->itemFactory->matches( ind, d.searchExpression() );
}

bool ClipboardBrowser::hideFiltered(int row)
{
    const bool hide = isFiltered(row);
    setRowHidden(row, hide);
    d.setRowVisible(row, !hide);
    return hide;
}

bool ClipboardBrowser::hideFiltered(const QModelIndex &index)
{
    return hideFiltered(index.row());
}

bool ClipboardBrowser::startEditor(QObject *editor, bool changeClipboard)
{
    connect( editor, SIGNAL(fileModified(QByteArray,QString,QModelIndex)),
             this, SLOT(itemModified(QByteArray,QString,QModelIndex)) );

    if (changeClipboard) {
        connect( editor, SIGNAL(fileModified(QByteArray,QString,QModelIndex)),
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

void ClipboardBrowser::setEditorWidget(ItemEditorWidget *editor, bool changeClipboard)
{
    bool active = editor != nullptr;

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
            connect( editor, SIGNAL(searchRequest()),
                     this, SIGNAL(searchRequest()) );
            updateEditorGeometry();
            editor->show();
            editor->setFocus();
        } else {
            setFocus();
            emit editingFinished();
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

    emit updateContextMenu(this);
    emit searchHideRequest();
}

void ClipboardBrowser::editItem(const QModelIndex &index, bool editNotes, bool changeClipboard)
{
    if (!index.isValid())
        return;

    ItemEditorWidget *editor = d.createCustomEditor(this, index, editNotes);
    if (editor != nullptr) {
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

QModelIndex ClipboardBrowser::indexNear(int offset) const
{
    return ::indexNear(this, offset);
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

    // Delegate receives model signals first to update internal item list.
    connect( &m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             &d, SLOT(rowsInserted(QModelIndex, int, int)) );
    connect( &m, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
             &d, SLOT(rowsRemoved(QModelIndex,int,int)) );
    connect( &m, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)),
             &d, SLOT(rowsMoved(QModelIndex, int, int, QModelIndex, int)) );
    connect( &m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             &d, SLOT(dataChanged(QModelIndex,QModelIndex)) );

    connect( &m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(onDataChanged(QModelIndex,QModelIndex)) );
    connect( &m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(onRowsInserted(QModelIndex, int, int)));

    // Item count change
    connect( &m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(onItemCountChanged()) );
    connect( &m, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             SLOT(onItemCountChanged()) );

    // Save on change
    connect( &m, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(delayedSaveItems()) );
    connect( &m, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
             SLOT(delayedSaveItems()) );
    connect( &m, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)),
             SLOT(delayedSaveItems()) );
    connect( &m, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(delayedSaveItems()) );
}

void ClipboardBrowser::updateItemMaximumSize()
{
    QSize minSize = viewport()->contentsRect().size();
    if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
         minSize -= QSize(verticalScrollBar()->width(), 0);
    if ( minSize.width() > 0 )
        d.setItemSizes(m_sharedData->textWrap ? minSize : QSize(2048, 2048), minSize.width());
}

void ClipboardBrowser::processDragAndDropEvent(QDropEvent *event)
{
    acceptDrag(event);
    m_dragTargetRow = getDropRow(event->pos());
}

int ClipboardBrowser::dropIndexes(const QModelIndexList &indexes)
{
    QList<QPersistentModelIndex> toRemove;
    toRemove.reserve( indexes.size() );

    for (const auto &index : indexes) {
        if ( index.isValid() )
            toRemove.append(index);
    }

    std::sort( std::begin(toRemove), std::end(toRemove) );

    const auto first = toRemove.value(0).row();

    // Remove ranges of rows instead of a single rows.
    for (auto it1 = std::begin(toRemove); it1 != std::end(toRemove); ) {
        if ( it1->isValid() ) {
            const auto firstRow = it1->row();
            auto rowCount = 0;

            for ( ++it1, ++rowCount; it1 != std::end(toRemove)
                  && it1->isValid()
                  && it1->row() == firstRow + rowCount; ++it1, ++rowCount ) {}

            m.removeRows(firstRow, rowCount);
        } else {
            ++it1;
        }
    }

    return first;
}

void ClipboardBrowser::focusEditedIndex()
{
    if ( !editing() )
        return;

    const auto index = m_editor->index();
    if ( index.isValid() )
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
}

int ClipboardBrowser::findNextVisibleRow(int row)
{
    while ( row < length() && isRowHidden(row) ) { ++row; }
    return row < length() ? row : -1;
}

int ClipboardBrowser::findPreviousVisibleRow(int row)
{
    while ( row >= 0 && isRowHidden(row) ) { --row; }
    return row >= 0 ? row : -1;
}

void ClipboardBrowser::preload(int pixelsAboveCurrent, int pixelsBelowCurrent, const QModelIndex &current)
{
    preload(pixelsAboveCurrent, true, current);
    preload(pixelsBelowCurrent, false, current);
}

void ClipboardBrowser::preload(int pixels, bool above, const QModelIndex &current)
{
    const int s = 2 * spacing();
    const int direction = above ? -1 : 1;
    int row = current.row() + direction;
    int y = 0;
    for ( auto ind = index(row); ind.isValid() && y < pixels; ind = index(row) ) {
        if ( isRowHidden(row) ) {
            d.invalidateCache(row);
        } else {
            itemWidget(ind);
            y += s + d.sizeHint(ind).height();
        }
        row += direction;
    }

    // Unload item widgets below the threshold (unloding pixels above would change the scroll offset).
    if (!above) {
        for ( auto ind = index(row); ind.isValid(); ind = index(row) ) {
            d.invalidateCache(row);
            row += direction;
        }
    }
}

void ClipboardBrowser::moveToTop(const QModelIndex &index)
{
    if ( !index.isValid() || !isLoaded() )
        return;

    const auto data = index.data(contentType::data).toMap();
    if ( m_itemSaver->canMoveItems(QList<QModelIndex>() << index) ) {
        m.removeRow( index.row() );
        m.insertItem(data, 0);
    }
}

QVariantMap ClipboardBrowser::copyIndex(const QModelIndex &index) const
{
    auto data = index.data(contentType::data).toMap();
    return m_itemSaver ? m_itemSaver->copyItem(m, data) : data;
}

QVariantMap ClipboardBrowser::copyIndexes(const QModelIndexList &indexes) const
{
    if (indexes.size() == 1)
        return copyIndex( indexes.first() );

    QByteArray bytes;
    QByteArray text;
    QByteArray uriList;
    QVariantMap data;
    QSet<QString> usedFormats;

    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        for (const auto &index : indexes) {
            auto itemData = index.data(contentType::data).toMap();
            itemData = m_itemSaver ? m_itemSaver->copyItem(m, itemData) : itemData;

            stream << itemData;

            appendTextData(itemData, mimeText, &text);
            appendTextData(itemData, mimeUriList, &uriList);

            for ( const auto &format : itemData.keys() ) {
                if ( usedFormats.contains(format) ) {
                    if ( format.startsWith(COPYQ_MIME_PREFIX) )
                        data[format].clear();
                    else
                        data.remove(format);
                } else {
                    data[format] = itemData[format];
                    usedFormats.insert(format);
                }
            }
        }
    }

    data.insert(mimeItems, bytes);

    if ( !text.isNull() ) {
        data.insert(mimeText, text);
        data.remove(mimeHtml);
    }

    if ( !uriList.isNull() )
        data.insert(mimeUriList, uriList);

    return data;
}

int ClipboardBrowser::removeIndexes(const QModelIndexList &indexes, QString *error)
{
    Q_ASSERT(m_itemSaver);

    if ( indexes.isEmpty() ) {
        if (error)
            *error = "No valid rows specified";
        return -1;
    }

    if ( !m_itemSaver->canRemoveItems(indexes, error) )
        return -1;

    m_itemSaver->itemsRemovedByUser(indexes);

    return dropIndexes(indexes);
}

void ClipboardBrowser::paste(const QVariantMap &data, int destinationRow)
{
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
    for (const auto &index : indexes) {
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
    for (const auto &index : indexes) {
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

void ClipboardBrowser::onDataChanged(const QModelIndex &, const QModelIndex &)
{
    if (!editing())
        emit updateContextMenu(this);
}

void ClipboardBrowser::onRowsInserted(const QModelIndex &, int first, int)
{
    if ( !hideFiltered(first) ) {
        selectionModel()->clearSelection();
        const auto newIndex = index(first);
        setCurrentIndex(newIndex);
    }
}

void ClipboardBrowser::onItemCountChanged()
{
    if (!m_timerEmitItemCount.isActive())
        m_timerEmitItemCount.start();
}

void ClipboardBrowser::onEditorDestroyed()
{
    setEditorWidget(nullptr);
    // Set the focus back on the browser
    setFocus();
}

void ClipboardBrowser::onEditorSave()
{
    Q_ASSERT(m_editor != nullptr);
    m_editor->commitData(&m);
    focusEditedIndex();
    saveItems();
}

void ClipboardBrowser::onEditorCancel()
{
    focusEditedIndex();

    if ( editing() && m_editor->hasFocus() )
        maybeCloseEditor();
    else
        emit searchHideRequest();
}

void ClipboardBrowser::onEditorNeedsChangeClipboard()
{
    QModelIndex index = m_editor->index();
    if (index.isValid())
        emit changeClipboard( copyIndex(index) );
}

void ClipboardBrowser::onEditorNeedsChangeClipboard(const QByteArray &bytes, const QString &mime)
{
    emit changeClipboard(createDataMap(mime, bytes));
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

    updateItemMaximumSize();
    updateEditorGeometry();
}

void ClipboardBrowser::showEvent(QShowEvent *event)
{
    if ( m.rowCount() > 0 && !d.hasCache(index(0)) )
        scrollToTop();

    QListView::showEvent(event);
}

void ClipboardBrowser::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    if (previous.isValid())
        d.setItemWidgetStatic(previous, true);

    if ( current.isValid() ) {
        int row = -1;
        if ( previous.isValid() && current < previous) {
            row = findPreviousVisibleRow(current.row());
            if (row == -1)
                row = findNextVisibleRow(current.row());
        } else {
            row = findNextVisibleRow(current.row());
            if (row == -1)
                row = findPreviousVisibleRow(current.row());
        }

        if ( row != -1 && row != current.row() ) {
            setCurrentIndex( index(row) );
            return;
        }

        itemWidget(current);

        // Preload next and previous pages so that up/down and page up/down keys scroll correctly.
        const int h = viewport()->contentsRect().height();
        preload(h, h, current);

        scheduleDelayedItemsLayout();
        executeDelayedItemsLayout();
    }

    QListView::currentChanged(current, previous);
}

void ClipboardBrowser::selectionChanged(const QItemSelection &selected,
                                        const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    emit updateContextMenu(this);
}

void ClipboardBrowser::focusInEvent(QFocusEvent *event)
{
    // Always focus active editor instead of list.
    if (editing()) {
        focusNextChild();
    } else {
        if ( !currentIndex().isValid() )
            setCurrent(0);
        QListView::focusInEvent(event);
    }
}

void ClipboardBrowser::dragEnterEvent(QDragEnterEvent *event)
{
    dragMoveEvent(event);

    // WORKAROUND: Update drag'n'drop when modifiers are pressed/released (QTBUG-57168).
    qApp->installEventFilter(this);
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

    processDragAndDropEvent(event);
    update();
}

void ClipboardBrowser::dropEvent(QDropEvent *event)
{
    processDragAndDropEvent(event);

    if (event->dropAction() == Qt::MoveAction && event->source() == this)
        return; // handled in mouseMoveEvent()

    const QVariantMap data = cloneData( *event->mimeData() );
    paste(data, m_dragTargetRow);
    m_dragTargetRow = -1;
}

void ClipboardBrowser::paintEvent(QPaintEvent *e)
{
    const auto current = currentIndex();
    if ( current.isValid() )
        d.setItemWidgetStatic(current, false);

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

    QModelIndex targetIndex = indexNear( m_dragStartPosition.y() );
    if ( !targetIndex.isValid() )
        return;

    QModelIndexList selected = selectedIndexes();
    if ( !selected.contains(targetIndex) ) {
        setCurrentIndex(targetIndex);
        selected.clear();
        selected.append(targetIndex);
    }

    QVariantMap data = copyIndexes(selected);

    auto drag = new QDrag(this);
    drag->setMimeData( createMimeData(data) );
    drag->setPixmap( renderItemPreview(selected, 150, 150) );

    TemporaryDragAndDropImage *temporaryImage =
            TemporaryDragAndDropImage::create(drag->mimeData(), this);

    // Save persistent indexes so after the items are dropped (and added) these indexes remain valid.
    QList<QPersistentModelIndex> indexesToRemove;
    for (const auto &index : selected)
        indexesToRemove.append(index);

    // Start dragging (doesn't block event loop).
    // Default action is "copy" which works for most apps,
    // "move" action is used only in item list by default.
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    qApp->removeEventFilter(this);

    if (dropAction == Qt::MoveAction) {
        selected.clear();

        for (const auto &index : indexesToRemove)
            selected.append(index);

        QWidget *target = qobject_cast<QWidget*>(drag->target());

        // Move items only if target is this app.
        if (target == this || target == viewport()) {
            for (const auto &index : indexesToRemove) {
                const int sourceRow = index.row();
                const int targetRow =
                        sourceRow < m_dragTargetRow ? m_dragTargetRow - 1 : m_dragTargetRow;
                m.move(sourceRow, targetRow);
                if (sourceRow > m_dragTargetRow)
                    ++m_dragTargetRow;
            }
        } else if ( target && target->window() == window()
                    && m_itemSaver->canMoveItems(selected) )
        {
            removeIndexes(selected);
        }
    }

    // Clear drag indicator.
    m_dragTargetRow = -1;
    update();

    event->accept();

    if (temporaryImage)
        temporaryImage->drop();
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
    if (editor == nullptr) {
        const QVariantMap data = copyIndex(index);
        if ( data.contains(mimeText) )
        {
            ItemEditor *itemEditor = new ItemEditor(data[mimeText].toByteArray(), mimeText, m_sharedData->editor, this);
            itemEditor->setIndex(index);
            editor = itemEditor;
        }
    }

    return editor != nullptr && startEditor(editor);
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
        std::unique_ptr<ClipboardDialog> clipboardDialog(
                    new ClipboardDialog(currentIndex(), &m, this) );
        clipboardDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        clipboardDialog->show();
        clipboardDialog.release();
    }
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

void ClipboardBrowser::itemModified(const QByteArray &bytes, const QString &mime, const QModelIndex &index)
{
    // add new item
    if ( !bytes.isEmpty() ) {
        const QVariantMap dataMap = createDataMap(mime, bytes);
        if (index.isValid())
            m.setData(index, dataMap, contentType::updateData);
        else
            add(dataMap);
        saveItems();
    }
}

void ClipboardBrowser::filterItems(const QRegExp &re)
{
    // Do nothing if same regexp was already set or both are empty (don't compare regexp options).
    if ( (d.searchExpression().isEmpty() && re.isEmpty()) || d.searchExpression() == re )
        return;

    if ( editing() )
        m_editor->search(re);

    d.setSearch(re);

    // If search string is a number, highlight item in that row.
    bool ok;
    m_filterRow = re.pattern().toInt(&ok);
    if (!ok)
        m_filterRow = -1;

    int row = 0;
    for ( ; row < length() && hideFiltered(row); ++row ) {}

    setCurrentIndex(index(row));

    for ( ; row < length(); ++row )
        hideFiltered(row);

    if ( ok && m_filterRow >= 0 && m_filterRow < m.rowCount() )
        setCurrentIndex( index(m_filterRow) );
}

void ClipboardBrowser::moveToClipboard(const QModelIndex &ind)
{
    if ( !ind.isValid() )
        return;

    const auto data = copyIndex(ind);

    if (m_sharedData->moveItemOnReturnKey && ind.row() != 0) {
        moveToTop(ind);
        scrollToTop();
    }

    emit changeClipboard(data);
}

void ClipboardBrowser::editNew(const QString &text, bool changeClipboard)
{
    if ( !isLoaded() )
        return;

    emit searchHideRequest();
    if ( add(text) )
        editItem(currentIndex(), false, changeClipboard);
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    // ignore any input if editing an item
    if ( editing() )
        return;

    // translate keys for vi mode
    if (m_sharedData->viMode && handleViKey(event, this))
        return;

    const auto mods = event->modifiers();

    // TODO: Why is this needed?
    if (mods != Qt::AltModifier) {
        // allow user defined shortcuts
        QListView::keyPressEvent(event);
        // search
        event->ignore();
    }
}

void ClipboardBrowser::setCurrent(int row)
{
    QModelIndex prev = currentIndex();
    int cur = prev.row();

    const int direction = cur <= row ? 1 : -1;

    // select first visible
    int i = m.getRowNumber(row);
    cur = i;
    while ( 0 <= i && i < length() && isRowHidden(i) ) {
        i = m.getRowNumber(i + direction);
        if ( i == 0 || i == m.rowCount() - 1 || i == cur)
            break;
    }
    if ( i < 0 || i >= length() || isRowHidden(i) )
        return;

    const auto ind = index(i);
    clearSelection();
    setCurrentIndex(ind);
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
    const int currentRow = removeIndexes(toRemove);
    if (currentRow != -1)
        setCurrent(currentRow);
}

void ClipboardBrowser::sortItems(const QModelIndexList &indexes)
{
    m.sortItems(indexes, &alphaSort);
}

void ClipboardBrowser::reverseItems(const QModelIndexList &indexes)
{
    m.sortItems(indexes, &reverseSort);
}

bool ClipboardBrowser::allocateSpaceForNewItems(int newItemCount)
{
    const auto targetRowCount = m_sharedData->maxItems - newItemCount;
    const auto toRemove = m.rowCount() - targetRowCount;
    if (toRemove <= 0)
        return true;

    QModelIndexList indexesToRemove;
    QString error;
    for (int row = m.rowCount() - 1; row >= 0 && indexesToRemove.size() < toRemove; --row) {
        const auto index = m.index(row);
        if ( m_itemSaver->canRemoveItems(QModelIndexList() << index, &error) )
            indexesToRemove.append(index);
    }

    if (indexesToRemove.size() < toRemove)
        return false;

    dropIndexes(indexesToRemove);
    return true;
}

bool ClipboardBrowser::add(const QString &txt, int row)
{
    return add( createDataMap(mimeText, txt), row );
}

bool ClipboardBrowser::add(const QVariantMap &data, int row)
{
    if ( !isLoaded() ) {
        loadItems();
        if ( !isLoaded() )
            return false;
    }

    // list size limit
    if ( !allocateSpaceForNewItems(1) ) {
        QMessageBox::information(
                    this, tr("Cannot Add New Items"),
                    tr("Tab is full. Failed to remove any items.") );
        return false;
    }

    // create new item
    const int newRow = row < 0 ? m.rowCount() : qMin(row, m.rowCount());
    m.insertItem(data, newRow);

    delayedSaveItems();

    return true;
}

void ClipboardBrowser::addUnique(const QVariantMap &data)
{
    if ( moveToTop(hash(data)) ) {
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
    newData.remove(mimeOutputTab);
    newData.remove(mimeSyncToClipboard);
    newData.remove(mimeSyncToSelection);

#ifdef HAS_MOUSE_SELECTIONS
    // When selecting text under X11, clipboard data may change whenever selection changes.
    // Instead of adding item for each selection change, this updates previously added item.
    if ( !isClipboardData(data)
         && newData.contains(mimeText)
         // Don't update edited item.
         && (!editing() || currentIndex().row() != 0)
         )
    {
        const QModelIndex firstIndex = index(0);
        const QVariantMap previousData = copyIndex(firstIndex);

        if ( previousData.contains(mimeText)
             && getTextData(newData).contains(getTextData(previousData))
             )
        {
            COPYQ_LOG("New item: Merging with top item");

            const QSet<QString> formatsToAdd = previousData.keys().toSet() - newData.keys().toSet();

            for (const auto &format : formatsToAdd)
                newData.insert(format, previousData[format]);

            if ( add(newData) ) {
                const bool reselectFirst = !editing() && hasFocus() && currentIndex().row() == 1;
                m.removeRow(1);

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

bool ClipboardBrowser::loadItems()
{
    if ( isLoaded() )
        return true;

    m_timerSave.stop();

    m.blockSignals(true);
    m_itemSaver = ::loadItems(m_tabName, m, m_sharedData->itemFactory, m_sharedData->maxItems);
    m.blockSignals(false);

    if ( !isLoaded() )
        return false;

    d.rowsInserted(QModelIndex(), 0, m.rowCount());
    if ( hasFocus() )
        setCurrent(0);
    onItemCountChanged();

    return true;
}

bool ClipboardBrowser::saveItems()
{
    m_timerSave.stop();

    if ( !isLoaded() || m_tabName.isEmpty() )
        return false;

    return ::saveItems(m_tabName, m, m_itemSaver);
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

    for ( const auto &ind : selectionModel()->selectedIndexes() )
        result += ind.data(Qt::EditRole).toString() + QString('\n');
    result.chop(1);

    return result;
}

void ClipboardBrowser::setTabName(const QString &tabName)
{
    m_tabName = tabName;
    saveItems();
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

QWidget *ClipboardBrowser::currentItemPreview()
{
    if (!isLoaded())
        return nullptr;

    const QModelIndex index = currentIndex();
    const bool antialiasing = m_sharedData->theme.isAntialiasingEnabled();
    ItemWidget *itemWidget =
            m_sharedData->itemFactory->createItem(index, this, antialiasing, false, true);
    QWidget *w = itemWidget->widget();

    d.highlightMatches(itemWidget);

    return w;
}

void ClipboardBrowser::findNext()
{
    if (editing())
        m_editor->findNext(d.searchExpression());
}

void ClipboardBrowser::findPrevious()
{
    if (editing())
        m_editor->findPrevious(d.searchExpression());
}

ItemWidget *ClipboardBrowser::itemWidget(const QModelIndex &index)
{
    return d.cache(index);
}

bool ClipboardBrowser::editing() const
{
    return m_editor != nullptr;
}

bool ClipboardBrowser::isLoaded() const
{
    return !m_sharedData->itemFactory || m_itemSaver || tabName().isEmpty();
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

// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardbrowser.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/temporaryfile.h"
#include "common/textdata.h"
#include "common/timer.h"
#include "gui/pixelratio.h"
#include "gui/theme.h"
#include "item/itemeditor.h"
#include "item/itemeditorwidget.h"
#include "item/itemfactory.h"
#include "item/itemstore.h"
#include "item/itemwidget.h"
#include "item/persistentdisplayitem.h"

#include <QApplication>
#include <QDrag>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMimeData>
#include <QProgressBar>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QTemporaryFile>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>

namespace {

enum class MoveType {
    Absolute,
    Relative
};

/// Save drag'n'drop image data in temporary file (required by some applications).
class TemporaryDragAndDropImage final : public QObject {
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
        initSingleShotTimer( &m_timerRemove, removeAfterDropSeconds * 1000, this, &TemporaryDragAndDropImage::deleteLater );
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


QList<QPersistentModelIndex> toPersistentModelIndexList(const QList<QModelIndex> &indexes)
{
    QList<QPersistentModelIndex> result;
    result.reserve( indexes.size() );

    for (const auto &index : indexes) {
        if ( index.isValid() )
            result.append(index);
    }

    return result;
}

void moveIndexes(QList<QPersistentModelIndex> &indexesToMove, int targetRow, ClipboardModel *model, MoveType moveType)
{
    if ( indexesToMove.isEmpty() )
        return;

    const auto minMaxPair = std::minmax_element( std::begin(indexesToMove), std::end(indexesToMove) );
    const auto start = minMaxPair.first->row();
    const auto end = minMaxPair.second->row();

    if (moveType == MoveType::Relative) {
        if (targetRow < 0 && start == 0)
            targetRow = model->rowCount();
        else if (targetRow > 0 && end == model->rowCount() - 1)
            targetRow = 0;
        else if (targetRow < 0)
            targetRow += start;
        else
            targetRow += end + 1;
    }

    if (start < targetRow)
        std::sort( std::begin(indexesToMove), std::end(indexesToMove) );
    else if (targetRow < end)
        std::sort( std::begin(indexesToMove), std::end(indexesToMove),
                   [](const QModelIndex &lhs, const QModelIndex &rhs) {
                       return lhs.row() >= rhs.row();
                   });
    else
        return;

    // Move ranges of rows instead of a single rows.
    for (auto it = std::begin(indexesToMove); it != std::end(indexesToMove); ) {
        if ( it->isValid() ) {
            const auto row = it->row();
            auto rowCount = 0;

            for ( ++it, ++rowCount; it != std::end(indexesToMove)
                  && it->isValid()
                  && std::abs(it->row() - row) == rowCount; ++it, ++rowCount ) {}

            if (row < targetRow)
                model->moveRows(QModelIndex(), row, rowCount, QModelIndex(), targetRow);
            else
                model->moveRows(QModelIndex(), row - rowCount + 1, rowCount, QModelIndex(), targetRow);
        } else {
            ++it;
        }
    }
}

void moveIndexes(const QModelIndexList &indexesToMove, int targetRow, ClipboardModel *model, MoveType moveType)
{
    auto indexesToMove2 = toPersistentModelIndexList(indexesToMove);
    moveIndexes(indexesToMove2, targetRow, model, moveType);
}

} // namespace

ClipboardBrowser::ClipboardBrowser(
        const QString &tabName,
        const ClipboardBrowserSharedPtr &sharedData,
        QWidget *parent)
    : QListView(parent)
    , m_itemSaver(nullptr)
    , m_tabName(tabName)
    , m_maxItemCount(sharedData->maxItems)
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

    initSingleShotTimer( &m_timerSave, 0, this, &ClipboardBrowser::saveItems );
    initSingleShotTimer( &m_timerEmitItemCount, 0, this, &ClipboardBrowser::emitItemCount );
    initSingleShotTimer( &m_timerUpdateSizes, 0, this, &ClipboardBrowser::updateSizes );
    initSingleShotTimer( &m_timerUpdateCurrent, 0, this, &ClipboardBrowser::updateCurrent );
    initSingleShotTimer( &m_timerPreload, 0, this, &ClipboardBrowser::preloadCurrentPage );

    m_timerDragDropScroll.setInterval(20);
    connect( &m_timerDragDropScroll, &QTimer::timeout,
             this, &ClipboardBrowser::dragDropScroll );

    // ScrollPerItem doesn't work well with hidden items
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    setAttribute(Qt::WA_MacShowFocusRect, false);

    setAcceptDrops(true);

    connectModelAndDelegate();

    m_sharedData->theme.decorateBrowser(this);
    updateItemMaximumSize();
}

ClipboardBrowser::~ClipboardBrowser()
{
    delete m_editor.data();
    saveUnsavedItems();
}

bool ClipboardBrowser::moveToTop(uint itemHash)
{
    const int row = m.findItem(itemHash);
    if (row < 0)
        return false;

    moveToTop( index(row) );
    return true;
}

void ClipboardBrowser::closeExternalEditor(QObject *editor, const QModelIndex &index)
{
    editor->disconnect(this);
    disconnect(editor);
    editor->deleteLater();

    Q_ASSERT(m_externalEditorsOpen > 0);
    --m_externalEditorsOpen;

    if ( index.isValid() && !isInternalEditorOpen() ) {
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        emit requestShow(this);
    }

    maybeEmitEditingFinished();
}

void ClipboardBrowser::emitItemCount()
{
    if (isLoaded())
        emit itemCountChanged( tabName(), length() );
}

bool ClipboardBrowser::eventFilter(QObject *obj, QEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(5,12,0)
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
#endif

    return QListView::eventFilter(obj, event);
}

bool ClipboardBrowser::isFiltered(int row) const
{
    const auto filter = d.itemFilter();
    if ( !filter || filter->matchesAll() || !m_itemSaver)
        return false;

    if ( filter->matchesNone() )
        return true;

    const QModelIndex ind = m.index(row);
    return m_filterRow != row
            && m_sharedData->itemFactory
            && !m_sharedData->itemFactory->matches(ind, *filter);
}

bool ClipboardBrowser::hideFiltered(int row)
{
    const bool hide = isFiltered(row);
    setRowHidden(row, hide);
    return hide;
}

bool ClipboardBrowser::startEditor(QObject *editor)
{
    connect( editor, SIGNAL(fileModified(QByteArray,QString,QModelIndex)),
             this, SLOT(itemModified(QByteArray,QString,QModelIndex)) );

    connect( editor, SIGNAL(closed(QObject*,QModelIndex)),
             this, SLOT(closeExternalEditor(QObject*,QModelIndex)) );

    connect( editor, SIGNAL(error(QString)),
             this, SIGNAL(error(QString)) );

    connect( this, &ClipboardBrowser::closeExternalEditors,
             editor, &QObject::deleteLater );

    bool retVal = false;
    bool result = QMetaObject::invokeMethod( editor, "start", Qt::DirectConnection,
                                             Q_RETURN_ARG(bool, retVal) );

    if (!retVal || !result) {
        closeExternalEditor(editor, QModelIndex());
        return false;
    }

    ++m_externalEditorsOpen;

    return true;
}

void ClipboardBrowser::setEditorWidget(ItemEditorWidget *editor, bool changeClipboard)
{
    bool active = editor != nullptr;

    if (m_editor != editor) {
        if (m_editor) {
            focusEditedIndex();
            m_editor->hide();
            m_editor->deleteLater();
        }

        m_editor = editor;
        if (active) {
            emit searchHideRequest();
            connect( editor, &ItemEditorWidget::save,
                     this, &ClipboardBrowser::onEditorSave );
            if (changeClipboard) {
                connect( editor, &ItemEditorWidget::save,
                         this, &ClipboardBrowser::setClipboardFromEditor );
            }
            connect( editor, &ItemEditorWidget::cancel,
                     this, &ClipboardBrowser::onEditorCancel );
            connect( editor, &ItemEditorWidget::invalidate,
                     this, &ClipboardBrowser::onEditorInvalidate );
            connect( editor, &ItemEditorWidget::searchRequest,
                     this, &ClipboardBrowser::searchRequest );
            updateEditorGeometry();
            editor->show();
            editor->setFocus();
        } else {
            setFocus();
            maybeEmitEditingFinished();
            const auto filter = d.itemFilter();
            if ( !filter || filter->matchesAll() )
                emit searchHideRequest();
            else
                emit searchShowRequest(filter->searchString());
        }

        emit internalEditorStateChanged(this);
    }

    clearFocus();
    setFocusProxy(m_editor);
    setFocus();

    setAcceptDrops(!active);

    // Hide scrollbars while editing.
    const auto scrollbarPolicy = active
            ? Qt::ScrollBarAlwaysOff
            : m_sharedData->theme.scrollbarPolicy();
    setVerticalScrollBarPolicy(scrollbarPolicy);
    setHorizontalScrollBarPolicy(scrollbarPolicy);
}

void ClipboardBrowser::editItem(
    const QModelIndex &index, const QString &format, bool changeClipboard)
{
    if (!index.isValid())
        return;

    ItemEditorWidget *editor = d.createCustomEditor(this, index, format);
    if (editor != nullptr && editor->isValid() ) {
        setEditorWidget(editor, changeClipboard);
    } else {
        delete editor;
        openEditor(index, format);
    }
}

void ClipboardBrowser::updateEditorGeometry()
{
    if ( isInternalEditorOpen() ) {
        const QRect contents = viewport()->contentsRect();
        const QMargins margins = contentsMargins();
        m_editor->parentWidget()->setGeometry( contents.translated(margins.left(), margins.top()) );
    }
}

void ClipboardBrowser::updateCurrentItem()
{
    const QModelIndex current = currentIndex();
    if ( !current.isValid() )
        return;

    d.setCurrentRow( current.row(), hasFocus() );
}

QModelIndex ClipboardBrowser::indexNear(int offset) const
{
    return ::indexNear(this, offset);
}

int ClipboardBrowser::getDropRow(QPoint position)
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
    connect( &m, &QAbstractItemModel::rowsInserted,
             &d, &ItemDelegate::rowsInserted );
    connect( &m, &QAbstractItemModel::rowsAboutToBeRemoved,
             &d, &ItemDelegate::rowsRemoved );
    connect( &m, &QAbstractItemModel::rowsAboutToBeMoved,
             &d, &ItemDelegate::rowsMoved );
    connect( &m, &QAbstractItemModel::dataChanged,
             &d, &ItemDelegate::dataChanged );

    connect( &m, &QAbstractItemModel::rowsInserted,
             this, &ClipboardBrowser::onRowsInserted);

    // Item count change
    connect( &m, &QAbstractItemModel::rowsInserted,
             this, &ClipboardBrowser::onItemCountChanged );
    connect( &m, &QAbstractItemModel::rowsRemoved,
             this, &ClipboardBrowser::onItemCountChanged );

    // Save on change
    connect( &m, &QAbstractItemModel::rowsInserted,
             this, [this]() { delayedSaveItems(m_sharedData->saveDelayMsOnItemAdded); } );
    connect( &m, &QAbstractItemModel::rowsRemoved,
             this, [this]() { delayedSaveItems(m_sharedData->saveDelayMsOnItemRemoved); } );
    connect( &m, &QAbstractItemModel::rowsMoved,
             this, [this]() { delayedSaveItems(m_sharedData->saveDelayMsOnItemMoved); } );
    connect( &m, &QAbstractItemModel::dataChanged,
             this, [this]() { delayedSaveItems(m_sharedData->saveDelayMsOnItemModified); } );

    connect( &d, &ItemDelegate::itemWidgetCreated,
             this, &ClipboardBrowser::itemWidgetCreated );

    connect( &d, &ItemDelegate::sizeHintChanged,
             this, [this](){ m_timerPreload.start(); } );
}

void ClipboardBrowser::updateItemMaximumSize()
{
    const int viewWidth = viewport()->contentsRect().width();
    if (viewWidth > 0)
        d.setItemSizes(m_sharedData->textWrap ? viewWidth : 2048, viewWidth);
}

void ClipboardBrowser::processDragAndDropEvent(QDropEvent *event)
{
    acceptDrag(event);
    m_dragTargetRow = getDropRow(event->pos());
    dragDropScroll();
}

void ClipboardBrowser::dropIndexes(const QModelIndexList &indexes)
{
    auto toRemove = toPersistentModelIndexList(indexes);
    std::sort( std::begin(toRemove), std::end(toRemove) );
    dropIndexes(toRemove);
}

void ClipboardBrowser::dropIndexes(const QList<QPersistentModelIndex> &indexes)
{
    const QPersistentModelIndex current = currentIndex();
    const int first = indexes.value(0).row();

    // Remove ranges of rows instead of a single rows.
    for (auto it1 = std::begin(indexes); it1 != std::end(indexes); ) {
        if ( it1->isValid() ) {
            const auto firstRow = it1->row();
            auto rowCount = 0;

            for ( ++it1, ++rowCount; it1 != std::end(indexes)
                  && it1->isValid()
                  && it1->row() == firstRow + rowCount; ++it1, ++rowCount ) {}

            m.removeRows(firstRow, rowCount);
        } else {
            ++it1;
        }
    }

    // If current item was removed, select next visible.
    if ( !current.isValid() )
        setCurrent( findVisibleRowFrom(first) );
}

void ClipboardBrowser::focusEditedIndex()
{
    if ( !isInternalEditorOpen() )
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

int ClipboardBrowser::findVisibleRowFrom(int row)
{
    const int visibleRow = findNextVisibleRow(row);
    if (visibleRow != -1)
        return visibleRow;
    return findPreviousVisibleRow(row);
}

void ClipboardBrowser::preloadCurrentPage()
{
    if ( !viewport()->isVisible() )
        return;

    if ( m_timerUpdateSizes.isActive() )
        updateSizes();

    executeDelayedItemsLayout();
    m_timerPreload.stop();

    const QRect rect = viewport()->contentsRect();
    const int top = rect.top();
    const auto firstVisibleIndex = indexNear(top);
    preload(rect.height(), 1, firstVisibleIndex);
}

void ClipboardBrowser::preload(int pixels, int direction, const QModelIndex &start)
{
    if ( !start.isValid() )
        return;

    const int s = spacing();
    int y = -d.sizeHint(start).height();
    for ( QModelIndex ind = start;
          ind.isValid() && y < pixels;
          ind = index(ind.row() + direction) )
    {
        if ( isIndexHidden(ind) )
            continue;

        d.createItemWidget(ind);
        y += d.sizeHint(ind).height() + 2 * s;
    }
}

void ClipboardBrowser::moveToTop(const QModelIndex &index)
{
    if ( !index.isValid() || !isLoaded() )
        return;

    if ( m_itemSaver->canMoveItems(QList<QModelIndex>() << index) )
        m.moveRow(QModelIndex(), index.row(), QModelIndex(), 0);
}

void ClipboardBrowser::maybeEmitEditingFinished()
{
    if ( !isInternalEditorOpen() && !isExternalEditorOpen() )
        emit editingFinished();
}

QModelIndex ClipboardBrowser::firstUnpinnedIndex() const
{
    if (!m_itemSaver)
        return QModelIndex();

    auto indexes = QModelIndexList() << QModelIndex();
    for (int row = 0; row < length(); ++row) {
        indexes[0] = index(row);
        if ( m_itemSaver->canMoveItems(indexes) )
            return indexes[0];
    }

    return QModelIndex();
}

void ClipboardBrowser::dragDropScroll()
{
    if (m_dragTargetRow == -1) {
        m_timerDragDropScroll.stop();
        return;
    }

    if (!m_timerDragDropScroll.isActive()) {
        m_timerDragDropScroll.start();
        return;
    }

    const auto y = mapFromGlobal(QCursor::pos()).y();
    const auto h = viewport()->contentsRect().height();
    const auto diff = h / 4;
    const auto scrollAmount =
            (h < y + diff) ? (y + diff - h) / 4
          : (y < diff) ? -(diff - y) / 4
          : 0;

    if (scrollAmount != 0) {
        QScrollBar *v = verticalScrollBar();
        v->setValue(v->value() + scrollAmount);
    } else {
        m_timerDragDropScroll.stop();
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

            for (auto it = itemData.constBegin(); it != itemData.constEnd(); ++it) {
                const auto &format = it.key();
                if ( usedFormats.contains(format) ) {
                    if ( format.startsWith(COPYQ_MIME_PREFIX) )
                        data[format].clear();
                    else
                        data.remove(format);
                } else {
                    data[format] = it.value();
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

void ClipboardBrowser::removeIndexes(const QModelIndexList &indexes, QString *error)
{
    Q_ASSERT(m_itemSaver);

    if ( indexes.isEmpty() ) {
        if (error)
            *error = "No valid rows specified";
    }

    if ( !canRemoveItems(indexes, error) )
        return;

    auto toRemove = toPersistentModelIndexList(indexes);
    std::sort( std::begin(toRemove), std::end(toRemove) );

    QPointer<QObject> self(this);
    bool canRemove = true;
    emit runOnRemoveItemsHandler(toRemove, &canRemove);
    if (!canRemove) {
        COPYQ_LOG("Item removal cancelled from script");
        return;
    }
    if (!self)
        return;

    m_itemSaver->itemsRemovedByUser(toRemove);

    dropIndexes(toRemove);
}

bool ClipboardBrowser::canRemoveItems(const QModelIndexList &indexes, QString *error)
{
    Q_ASSERT(m_itemSaver);

    return m_itemSaver->canRemoveItems(indexes, error);
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

    const auto ratio = pixelRatio(this);
    const int frameLineWidth = static_cast<int>( std::ceil(2 * ratio) );

    const auto height = qMin(maxHeight, h + s + 2 * frameLineWidth);
    const auto width = qMin(maxWidth, viewport()->contentsRect().width() + 2 * frameLineWidth);
    const QSize size(width, height);

    QPixmap pix(size * ratio);
    pix.setDevicePixelRatio(ratio);

    QPainter p(&pix);

    h = frameLineWidth;
    const QPoint pos = viewport()->pos();
    for (const auto &index : indexes) {
        if ( isIndexHidden(index) )
            continue;
        const QPoint position(frameLineWidth, h);
        const auto rect = visualRect(index).translated(pos).adjusted(-s, -s, 2 * s, 2 * s);
        render(&p, position, rect);
        h += visualRect(index).height() + s;
        if ( h > height )
            break;
    }

    p.setBrush(Qt::NoBrush);

    const auto x = frameLineWidth / 2;
    const QRect rect(x / 2, x / 2, width - x, height - x);

    QPen pen(Qt::black, x, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    p.setPen(pen);
    p.drawRect(rect);

    pen.setColor(Qt::white);
    p.setPen(pen);
    p.drawRect( rect.adjusted(x, x, -x, -x) );

    return pix;
}

void ClipboardBrowser::onRowsInserted(const QModelIndex &, int first, int last)
{
    QModelIndex current;
    QItemSelection selection;

    // Select new items only if explicitly asked for
    // or select new top item when not actively using the item list.
    const bool select = m_selectNewItems
        || !currentIndex().isValid()
        || (first == 0
            && !isInternalEditorOpen()
            && (currentIndex().row() == last + 1
                || !isVisible()
                || !isActiveWindow()));

    // Avoid selecting multiple items if not requested.
    if (!m_selectNewItems)
        last = first;

    for (int row = first; row <= last; ++row) {
        if ( !hideFiltered(row) ) {
            const auto newIndex = index(row);
            if ( !current.isValid() )
                current = newIndex;
            if (select)
                selection.select(newIndex, newIndex);
        }
    }

    if ( !selection.isEmpty() ) {
        setCurrentIndex(current);
        selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    }
}

void ClipboardBrowser::onItemCountChanged()
{
    if (!m_timerEmitItemCount.isActive())
        m_timerEmitItemCount.start();
}

void ClipboardBrowser::onEditorSave()
{
    Q_ASSERT(!m_editor.isNull());

    if ( m_editor && m_editor->hasChanges() ) {
        const QVariantMap data = m_editor->data();
        if ( m_sharedData->itemFactory->setData(data, m_editor->index(), &m) )
            m_editor->setHasChanges(false);
    }

    focusEditedIndex();
    delayedSaveItems(m_sharedData->saveDelayMsOnItemEdited);
}

void ClipboardBrowser::onEditorCancel()
{
    if ( isInternalEditorOpen() && m_editor->hasFocus() ) {
        maybeCloseEditors();
    } else {
        emit searchHideRequest();
    }
}

void ClipboardBrowser::onEditorInvalidate()
{
    setEditorWidget(nullptr);
}

void ClipboardBrowser::setClipboardFromEditor()
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
    if ( isInternalEditorOpen() || selectedIndexes().isEmpty() )
        return;

    QPoint pos = event->globalPos();
    emit showContextMenu(pos);
}

void ClipboardBrowser::resizeEvent(QResizeEvent *event)
{
    // WORKAROUND: Omit calling resizeEvent() recursively.
    if (!m_resizeEvent) {
        m_resizeEvent = true;
        QListView::resizeEvent(event);
        m_resizeEvent = false;
    }

    m_timerUpdateSizes.start();
    m_timerPreload.start();
}

void ClipboardBrowser::showEvent(QShowEvent *event)
{
    preloadCurrentPage();
    QListView::showEvent(event);
}

void ClipboardBrowser::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QListView::currentChanged(current, previous);

    if (previous.isValid())
        d.setItemWidgetCurrent(previous, false);

    m_timerUpdateCurrent.start();
}

void ClipboardBrowser::selectionChanged(const QItemSelection &selected,
                                        const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    for ( auto index : selected.indexes() )
        d.setItemWidgetSelected(index, true);
    for ( auto index : deselected.indexes() )
        d.setItemWidgetSelected(index, false);
    emit itemSelectionChanged(this);
}

void ClipboardBrowser::focusInEvent(QFocusEvent *event)
{
    // Always focus active editor instead of list.
    if (isInternalEditorOpen()) {
        focusNextChild();
    } else {
        if ( !currentIndex().isValid() )
            setCurrent(0);
        QListView::focusInEvent(event);
        updateCurrentItem();
    }

    if (m_itemSaver)
        m_itemSaver->setFocus(true);
}

void ClipboardBrowser::focusOutEvent(QFocusEvent *event)
{
    QListView::focusOutEvent(event);

    if (m_itemSaver)
        m_itemSaver->setFocus(false);
}

void ClipboardBrowser::dragEnterEvent(QDragEnterEvent *event)
{
    dragMoveEvent(event);

#if QT_VERSION < QT_VERSION_CHECK(5,12,0)
    // WORKAROUND: Update drag'n'drop when modifiers are pressed/released (QTBUG-57168).
    qApp->installEventFilter(this);
#endif
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
    addAndSelect(data, m_dragTargetRow);
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
        // WORKAROUND: After double-click, if window is hidden while mouse button is pressed,
        //             release button event is received after window is show which can result
        //             in items being selected.
        if (m_ignoreMouseMoveWithButtonPressed) {
            if ( QApplication::mouseButtons() & Qt::LeftButton )
                return;
            m_ignoreMouseMoveWithButtonPressed = false;
        }

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

    m_dragStartPosition = QPoint();
    auto drag = new QDrag(this);
    drag->setMimeData( createMimeData(data) );
    drag->setPixmap( renderItemPreview(selected, 150, 150) );

    TemporaryDragAndDropImage *temporaryImage =
            TemporaryDragAndDropImage::create(drag->mimeData(), this);

    // Save persistent indexes so after the items are dropped (and added) these indexes remain valid.
    auto indexesToRemove = toPersistentModelIndexList(selected);

    // Start dragging (doesn't block event loop).
    // Default action is "copy" which works for most apps,
    // "move" action is used only in item list by default.
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
#if QT_VERSION < QT_VERSION_CHECK(5,12,0)
    qApp->removeEventFilter(this);
#endif

    if (dropAction == Qt::MoveAction) {
        selected.clear();

        for (const auto &index : indexesToRemove)
            selected.append(index);

        QWidget *target = qobject_cast<QWidget*>(drag->target());

        QPointer<QObject> self(this);

        // Move items only if target is this app.
        if (target == this || target == viewport()) {
            moveIndexes(indexesToRemove, m_dragTargetRow, &m, MoveType::Absolute);
        } else if ( target && target->window() == window()
                    && m_itemSaver->canRemoveItems(selected, nullptr) )
        {
            removeIndexes(selected);
        }

        if (!self)
            return;
    }

    // Clear drag indicator.
    m_dragTargetRow = -1;
    update();

    event->accept();

    if (temporaryImage)
        temporaryImage->drop();
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
void ClipboardBrowser::enterEvent(QEnterEvent *event)
#else
void ClipboardBrowser::enterEvent(QEvent *event)
#endif
{
    m_ignoreMouseMoveWithButtonPressed = true;
    QListView::enterEvent(event);
}

void ClipboardBrowser::scrollContentsBy(int dx, int dy)
{
    QListView::scrollContentsBy(dx, dy);
    if ( !m_timerPreload.isActive() )
        preloadCurrentPage();
}

void ClipboardBrowser::doItemsLayout()
{
    // Keep visible current item (or the first one visible)
    // on the same position visually after relayout.

    // FIXME: Virtual method QListView::doItemsLayout() is undocumented
    //        so other way should be used instead.

    m_timerPreload.start();

    const auto current = currentIndex();
    const auto currentRect = visualRect(current);
    const auto viewRect = viewport()->contentsRect();

    const bool currentIsNotVisible =
            viewRect.bottom() < currentRect.top()
            || currentRect.bottom() < viewRect.top();
    const auto index = currentIsNotVisible ? indexNear(0) : current;

    const auto rectBefore = visualRect(index);

    QListView::doItemsLayout();

    const auto rectAfter = visualRect(index);
    const auto offset = rectAfter.top() - rectBefore.top();
    if (offset != 0) {
        QScrollBar *v = verticalScrollBar();
        v->setValue(v->value() + offset);
        d.updateAllRows();
    }
}

bool ClipboardBrowser::openEditor()
{
    const QModelIndexList selected = selectionModel()->selectedRows();
    return (selected.size() == 1) ? openEditor( selected.first(), mimeText )
                                  : openEditor( {}, mimeText, selectedText().toUtf8() );
}

bool ClipboardBrowser::openEditor(
    const QModelIndex &index, const QString &format, const QByteArray &content, bool changeClipboard)
{
    if ( !isLoaded() )
        return false;

    if ( m_sharedData->editor.isEmpty() )
        return false;

    auto editor = new ItemEditor(content, format, m_sharedData->editor, this);
    editor->setIndex(index);
    if ( !startEditor(editor) )
        return false;

    if (changeClipboard) {
        connect( editor, SIGNAL(fileModified(QByteArray,QString,QModelIndex)),
                 this, SLOT(onEditorNeedsChangeClipboard(QByteArray,QString)) );
    }

    return true;
}

bool ClipboardBrowser::openEditor(const QModelIndex &index, const QString &format)
{
    if ( !isLoaded() )
        return false;

    const auto data = m_sharedData->itemFactory->data(index);
    if ( data.isEmpty() )
        return false;

    QObject *editor = m_sharedData->itemFactory->createExternalEditor(index, data, this);
    if ( editor != nullptr && startEditor(editor) )
        return true;

    if ( !m_sharedData->editor.trimmed().isEmpty() ) {
        const QString text = getTextData(data);
        if ( !text.isNull() ) {
            auto itemEditor = new ItemEditor( text.toUtf8(), format, m_sharedData->editor, this );
            itemEditor->setIndex(index);
            if ( startEditor(itemEditor) )
                return true;
        }
    }

    return false;
}

void ClipboardBrowser::editNotes()
{
    QModelIndex ind = currentIndex();
    if ( !ind.isValid() )
        return;

    scrollTo(ind, PositionAtTop);
    emit requestShow(this);

    editItem(ind, mimeItemNotes);
}

void ClipboardBrowser::itemModified(const QByteArray &bytes, const QString &mime, const QModelIndex &index)
{
    // add new item
    if ( !bytes.isEmpty() ) {
        const QVariantMap dataMap = createDataMap(mime, bytes);
        if (index.isValid())
            m_sharedData->itemFactory->setData(dataMap, index, &m);
        else
            add(dataMap);
    }
}

void ClipboardBrowser::filterItems(const ItemFilterPtr &filter)
{
    // Search in editor if open.
    if ( isInternalEditorOpen() ) {
        m_editor->search(filter);
        return;
    }

    // Do nothing if same regexp was already set.
    // FIXME: Compare other options.
    const auto oldFilter = d.itemFilter();
    const auto oldSearch = oldFilter ? oldFilter->searchString() : QString();
    const auto newSearch = filter ? filter->searchString() : QString();
    if (oldSearch == newSearch)
        return;

    d.setItemFilter(filter);

    // If search string is a number, highlight item in that row.
    bool filterByRowNumber = !m_sharedData->numberSearch;
    if (filterByRowNumber) {
        m_filterRow = newSearch.toInt(&filterByRowNumber);
        if (m_filterRow > 0 && m_sharedData->rowIndexFromOne)
            --m_filterRow;
    }
    if (!filterByRowNumber)
        m_filterRow = -1;

    int row = 0;

    if ( !filter || filter->matchesAll() ) {
        for ( ; row < length(); ++row )
            hideFiltered(row);

        scrollTo(currentIndex(), PositionAtCenter);
    } else {
        for ( ; row < length() && hideFiltered(row); ++row ) {}

        setCurrent(row);

        for ( ; row < length(); ++row )
            hideFiltered(row);

        if ( filterByRowNumber && m_filterRow >= 0 && m_filterRow < m.rowCount() )
            setCurrent(m_filterRow);
    }

    d.updateAllRows();
}

void ClipboardBrowser::moveToClipboard(const QModelIndex &ind)
{
    if ( ind.isValid() )
        moveToClipboard(QModelIndexList() << ind);
}

void ClipboardBrowser::moveToClipboard(const QModelIndexList &indexes)
{
    const auto data = copyIndexes(indexes);

    if ( m_sharedData->moveItemOnReturnKey
         && m_itemSaver && m_itemSaver->canMoveItems(indexes) )
    {
        moveIndexes(indexes, 0, &m, MoveType::Absolute);
        scrollTo( currentIndex() );
    }

    emit changeClipboard(data);
}

void ClipboardBrowser::editNew(const QString &format, const QByteArray &content, bool changeClipboard)
{
    if ( !isLoaded() )
        return;

    emit searchHideRequest();
    filterItems(nullptr);

    m_selectNewItems = true;
    const bool added = add( createDataMap(format, content) );
    m_selectNewItems = false;

    if (added)
        editItem(currentIndex(), format, changeClipboard);
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    // ignore any input if editing an item
    if ( isInternalEditorOpen() )
        return;

    // translate keys for vi mode
    if (m_sharedData->viMode && handleViKey(event, this)) {
        d.updateIfNeeded();
        return;
    }

    const Qt::KeyboardModifiers mods = event->modifiers();

    if (mods == Qt::AltModifier)
        return; // Handled by filter completion popup.

    const int key = event->key();

    // This fixes few issues with default navigation and item selections.
    switch (key) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageDown:
    case Qt::Key_PageUp: {
        event->accept();

        const QModelIndex current = currentIndex();
        const int h = viewport()->contentsRect().height();
        const int s = spacing();
        const int space = 2 * s + 1;
        const int direction =
            (key == Qt::Key_Down || key == Qt::Key_PageDown || key == Qt::Key_End) ? 1 : -1;
        int row = current.row();

        if (key == Qt::Key_PageDown || key == Qt::Key_PageUp) {
            const int offset = verticalOffset();
            const QRect currentRect = rectForIndex(current);
            if (currentRect.bottom() < offset || currentRect.top() > offset + h
                || (key == Qt::Key_PageDown
                    ? currentRect.bottom() > offset + h
                    : currentRect.top() < offset))
            {
                QScrollBar *v = verticalScrollBar();
                v->setValue( v->value() + direction * v->pageStep() );
                break;
            }

            QModelIndex ind = indexNear(s + 1);
            row = ind.row();
            int y;
            if (key == Qt::Key_PageDown)
                y = rectForIndex(ind).top() - offset - h;
            else
                y = offset - rectForIndex(ind).top() + s - h;

            for ( ; ind.isValid(); row += direction, ind = index(row) ) {
                if ( isIndexHidden(ind) )
                    continue;

                d.createItemWidget(ind);
                y += d.sizeHint(ind).height() + 2 * s;
                if (y > space)
                    break;
            }

            if (row == current.row())
                row += direction;
            else if (row != current.row() + direction)
                row -= direction;
        } else if (key == Qt::Key_Up || key == Qt::Key_Down) {
            preload(space, direction, current);
            row += direction;
        } else {
            row = (key == Qt::Key_Home) ? 0 : model()->rowCount() - 1;
            preload(h, -direction, index(row));
            for ( ; row != current.row() && hideFiltered(row); row -= direction ) {}
        }

        executeDelayedItemsLayout();

        const QItemSelectionModel::SelectionFlags flags = selectionCommand(index(row), event);
        const bool setCurrentOnly = flags.testFlag(QItemSelectionModel::NoUpdate);
        const bool keepSelection = setCurrentOnly || flags.testFlag(QItemSelectionModel::SelectCurrent);

        setCurrent(row, keepSelection, setCurrentOnly);

        if (key == Qt::Key_PageDown || key == Qt::Key_PageUp)
            scrollTo(index(row), PositionAtTop);

        break;
    }

    default:
        // allow user defined shortcuts
        QListView::keyPressEvent(event);
        // search
        event->ignore();
        break;
    }

    d.updateIfNeeded();
}

void ClipboardBrowser::setCurrent(int row, bool keepSelection, bool setCurrentOnly)
{
    if ( m.rowCount() == 0 )
        return;

    QModelIndex prev = currentIndex();
    int cur = prev.row();

    const int direction = cur <= row ? 1 : -1;

    // select first visible
    int toSelect = std::clamp(row, 0, m.rowCount() - 1);
    toSelect = direction == 1
        ? findNextVisibleRow(toSelect)
        : findPreviousVisibleRow(toSelect);
    if (toSelect == -1)
        return;

    if (keepSelection) {
        auto sel = selectionModel();
        const bool currentSelected = sel->isSelected(prev);
        for ( int j = prev.row(); j != toSelect + direction; j += direction ) {
            const auto ind = index(j);
            if ( !ind.isValid() )
                break;
            if ( isRowHidden(j) )
                continue;

            if (!setCurrentOnly) {
                if ( currentIndex() != ind && sel->isSelected(ind) && sel->isSelected(prev) )
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
        setCurrentIndex( index(toSelect) );
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
            editItem(ind, mimeText);
        }
    }
}

void ClipboardBrowser::remove()
{
    const QModelIndexList toRemove = selectedIndexes();
    removeIndexes(toRemove);
}

void ClipboardBrowser::sortItems(const QModelIndexList &indexes)
{
    m.sortItems(indexes, &alphaSort);
}

void ClipboardBrowser::sortItems(const QList<QPersistentModelIndex> &sorted)
{
    m.sortItems(sorted);
}

void ClipboardBrowser::reverseItems(const QModelIndexList &indexes)
{
    m.sortItems(indexes, &reverseSort);
}

bool ClipboardBrowser::allocateSpaceForNewItems(int newItemCount)
{
    const auto targetRowCount = m_maxItemCount - newItemCount;
    const auto toRemove = m.rowCount() - targetRowCount;
    if (toRemove <= 0)
        return true;

    QModelIndexList indexesToRemove;
    for (int row = m.rowCount() - 1; row >= 0 && indexesToRemove.size() < toRemove; --row) {
        const auto index = m.index(row);
        if ( m_itemSaver->canDropItem(index) )
            indexesToRemove.append(index);
    }

    if (indexesToRemove.size() < toRemove) {
        log( QString("Cannot add new items. Tab \"%1\" reached the maximum number of items.")
             .arg(m_tabName), LogWarning );
        emit error(
            tr("Cannot add new items to tab %1. Please remove items manually to make space.")
            .arg(quoteString(m_tabName)) );
        return false;
    }

    dropIndexes(indexesToRemove);
    return true;
}

bool ClipboardBrowser::add(const QString &txt, int row)
{
    return add( createDataMap(mimeText, txt), row );
}

bool ClipboardBrowser::add(const QVariantMap &data, int row)
{
    if ( !isLoaded() )
        return false;

    const int newRow = row < 0 ? m.rowCount() : qMin(row, m.rowCount());

    if ( data.contains(mimeItems) ) {
        const QByteArray bytes = data[mimeItems].toByteArray();
        QDataStream stream(bytes);

        QVector<QVariantMap> dataList;
        while ( !stream.atEnd() ) {
            QVariantMap dataMap;
            stream >> dataMap;
            dataList.append(dataMap);
        }

        if ( !allocateSpaceForNewItems(dataList.size()) )
            return false;

        m.insertItems(dataList, newRow);
    } else {
        if ( !allocateSpaceForNewItems(1) )
            return false;

        m.insertItem(data, newRow);
    }

    return true;
}

bool ClipboardBrowser::addReversed(const QVector<QVariantMap> &dataList, int row)
{
    if ( !isLoaded() )
        return false;

    const int newRow = row < 0 ? m.rowCount() : qMin(row, m.rowCount());

    QVector<QVariantMap> items;
    items.reserve(dataList.size());
    for (auto it = std::rbegin(dataList); it != std::rend(dataList); ++it) {
        if ( it->contains(mimeItems) ) {
            const QByteArray bytes = (*it)[mimeItems].toByteArray();
            QDataStream stream(bytes);

            while ( !stream.atEnd() ) {
                QVariantMap dataMap;
                stream >> dataMap;
                items.append(dataMap);
            }
        } else {
            items.append(*it);
        }
    }

    if ( !allocateSpaceForNewItems(items.size()) )
        return false;

    m.insertItems(items, newRow);
    return true;
}

bool ClipboardBrowser::addAndSelect(const QVariantMap &data, int row)
{
    m_selectNewItems = true;
    bool added = add(data, row);
    m_selectNewItems = false;
    return added;
}

void ClipboardBrowser::addUnique(const QVariantMap &data, ClipboardMode mode)
{
    if ( moveToTop(hash(data)) ) {
        COPYQ_LOG("New item: Moving existing to top");
        return;
    }

    // When selecting text under X11, clipboard data may change whenever selection changes.
    // Instead of adding item for each selection change, this updates previously added item.
    // Also update previous item if the same selected text is copied to clipboard afterwards.
    if ( data.contains(mimeText) ) {
        const auto firstIndex = firstUnpinnedIndex();
        const QVariantMap previousData = firstIndex.data(contentType::data).toMap();

        if ( firstIndex.isValid()
             && previousData.contains(mimeText)
             // Don't update edited item.
             && (!isInternalEditorOpen() || currentIndex() != firstIndex) )
        {
            const auto newText = getTextData(data);
            const auto oldText = getTextData(previousData);
            if ( (mode == ClipboardMode::Clipboard)
                 ? (newText == oldText)
                 : (newText.startsWith(oldText) || newText.endsWith(oldText)) )
            {
                COPYQ_LOG("New item: Merging with top item");

                auto newData = previousData;
                for (auto it = data.constBegin(); it != data.constEnd(); ++it)
                    newData.insert(it.key(), it.value());

                m.setData(firstIndex, newData, contentType::data);

                return;
            }
        }
    }

    COPYQ_LOG("New item: Adding");

    add(data);
}

void ClipboardBrowser::setItemsData(const QMap<QPersistentModelIndex, QVariantMap> &itemsData)
{
    if ( isLoaded() )
        m.setItemsData(itemsData);
}

bool ClipboardBrowser::loadItems()
{
    if ( isLoaded() )
        return true;

    m_timerSave.stop();

    m.blockSignals(true);
    m_itemSaver = ::loadItems(m_tabName, m, m_sharedData->itemFactory, m_maxItemCount);
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

    if (!m_storeItems)
        return true;

    return ::saveItems(m_tabName, m, m_itemSaver);
}

void ClipboardBrowser::moveToClipboard()
{
    moveToClipboard( selectionModel()->selectedIndexes() );
}

void ClipboardBrowser::delayedSaveItems(int ms)
{
    if ( !isLoaded() || tabName().isEmpty() || ms < 0 )
        return;

    if ( !m_timerSave.isActive() || ms < m_timerSave.remainingTime() )
        m_timerSave.start(ms);

    emit itemsChanged(this);
}

void ClipboardBrowser::updateSizes()
{
    if (m_resizing) {
        m_timerUpdateSizes.start();
        return;
    }

    m_timerUpdateSizes.stop();

    m_resizing = true;
    updateItemMaximumSize();
    updateEditorGeometry();
    m_resizing = false;
}

void ClipboardBrowser::updateCurrent()
{
    if ( !updatesEnabled() ) {
        m_timerUpdateCurrent.start();
        return;
    }

    const auto current = currentIndex();
    if ( current.isValid() )
        d.setItemWidgetCurrent(current, true);
}

void ClipboardBrowser::saveUnsavedItems()
{
    if ( m_timerSave.isActive() )
        saveItems();
}

const QString ClipboardBrowser::selectedText() const
{
    QString result;

    for ( const auto &ind : selectionModel()->selectedIndexes() )
        result += ind.data(Qt::EditRole).toString() + QString('\n');
    result.chop(1);

    return result;
}

bool ClipboardBrowser::setTabName(const QString &tabName)
{
    const QString oldTabName = m_tabName;

    m_tabName = tabName;
    if ( saveItems() )
        return true;

    m_tabName = oldTabName;
    return false;
}

void ClipboardBrowser::setMaxItemCount(int count)
{
    m_maxItemCount = count;
}

void ClipboardBrowser::setStoreItems(bool store)
{
    m_storeItems = store;
    if (!m_storeItems)
        ::removeItems(m_tabName);
}

void ClipboardBrowser::editRow(int row, const QString &format)
{
    editItem( index(row), format );
}

void ClipboardBrowser::move(int key)
{
    if (key == Qt::Key_Home)
        moveIndexes( selectedIndexes(), 0, &m, MoveType::Absolute );
    else if (key == Qt::Key_End)
        moveIndexes( selectedIndexes(), m.rowCount(), &m, MoveType::Absolute );
    else if (key == Qt::Key_Down)
        moveIndexes( selectedIndexes(), 1, &m, MoveType::Relative );
    else if (key == Qt::Key_Up)
        moveIndexes( selectedIndexes(), -1, &m, MoveType::Relative );
    scrollTo( currentIndex() );
}

void ClipboardBrowser::move(const QModelIndexList &indexes, int targetRow)
{
    moveIndexes( indexes, targetRow, &m, MoveType::Absolute );
}

QWidget *ClipboardBrowser::currentItemPreview(QWidget *parent)
{
    if (!isLoaded())
        return nullptr;

    const QModelIndex index = currentIndex();
    const auto data = index.data(contentType::data).toMap();
    return d.createPreview(data, parent);
}

void ClipboardBrowser::findNext()
{
    if (isInternalEditorOpen())
        m_editor->findNext();
}

void ClipboardBrowser::findPrevious()
{
    if (isInternalEditorOpen())
        m_editor->findPrevious();
}

bool ClipboardBrowser::isInternalEditorOpen() const
{
    return !m_editor.isNull();
}

bool ClipboardBrowser::isExternalEditorOpen() const
{
    return m_externalEditorsOpen > 0;
}

bool ClipboardBrowser::isLoaded() const
{
    return !m_sharedData->itemFactory || m_itemSaver || tabName().isEmpty();
}

bool ClipboardBrowser::maybeCloseEditors()
{
    if ( (isInternalEditorOpen() && m_editor->hasChanges())
         || isExternalEditorOpen() )
    {
        const int answer = QMessageBox::question( this,
                    tr("Discard Changes?"),
                    tr("Do you really want to <strong>discard changes</strong>?"),
                    QMessageBox::No | QMessageBox::Yes,
                    QMessageBox::No );
        if (answer == QMessageBox::No)
            return false;
    }

    setEditorWidget(nullptr);

    emit closeExternalEditors();

    return true;
}

void ClipboardBrowser::keyboardSearch(const QString &text)
{
    emit searchShowRequest(text);
}

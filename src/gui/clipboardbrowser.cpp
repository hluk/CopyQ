/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "common/common.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/temporaryfile.h"
#include "common/textdata.h"
#include "common/timer.h"
#include "gui/clipboarddialog.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
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

int removeIndexes(const QModelIndexList &indexes, QAbstractItemModel *model)
{
    auto toRemove = toPersistentModelIndexList(indexes);
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

            model->removeRows(firstRow, rowCount);
        } else {
            ++it1;
        }
    }

    return first;
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
        std::sort( std::begin(indexesToMove), std::end(indexesToMove), std::not2(std::less<QModelIndex>()) );
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
        setCurrentIndex(index);
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

    return QListView::eventFilter(obj, event);
}

bool ClipboardBrowser::isFiltered(int row) const
{
    const auto re = d.searchExpression();
    if ( re.pattern().isEmpty() || !m_itemSaver)
        return false;

    if ( !re.isValid() )
        return true;

    const QModelIndex ind = m.index(row);
    return m_filterRow != row
            && m_sharedData->itemFactory
            && !m_sharedData->itemFactory->matches(ind, re);
}

QVariantMap ClipboardBrowser::itemData(const QModelIndex &index) const
{
    return index.data(contentType::data).toMap();
}

bool ClipboardBrowser::hideFiltered(int row)
{
    const bool hide = isFiltered(row);
    setRowHidden(row, hide);

    auto w = d.cacheOrNull(row);
    if (w) {
        if (hide)
            w->widget()->hide();
        else
            d.highlightMatches(w);
    }

    return hide;
}

bool ClipboardBrowser::hideFiltered(const QModelIndex &index)
{
    return hideFiltered(index.row());
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
            const auto oldSearch = d.searchExpression().pattern();
            if ( oldSearch.isEmpty() )
                emit searchHideRequest();
            else
                emit searchShowRequest(oldSearch);
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

void ClipboardBrowser::editItem(const QModelIndex &index, bool editNotes, bool changeClipboard)
{
    if (!index.isValid())
        return;

    ItemEditorWidget *editor = d.createCustomEditor(this, index, editNotes);
    if (editor != nullptr && editor->isValid() ) {
        setEditorWidget(editor, changeClipboard);
    } else {
        delete editor;
        if (!editNotes)
            openEditor(index);
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

    ItemWidget *item = d.cacheOrNull(current.row());
    if (!item)
        return;

    item->setCurrent(false);
    item->setCurrent( hasFocus() );
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
}

void ClipboardBrowser::updateItemMaximumSize()
{
    const QSize minSize = viewport()->contentsRect().size();
    if ( minSize.width() > 0 )
        d.setItemSizes(m_sharedData->textWrap ? minSize : QSize(2048, 2048), minSize.width());
}

void ClipboardBrowser::processDragAndDropEvent(QDropEvent *event)
{
    acceptDrag(event);
    m_dragTargetRow = getDropRow(event->pos());
    dragDropScroll();
}

int ClipboardBrowser::dropIndexes(const QModelIndexList &indexes)
{
    return ::removeIndexes(indexes, &m);
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

void ClipboardBrowser::preloadCurrentPage()
{
    const int h = viewport()->contentsRect().height();
    const QModelIndex start = indexNear(0);
    preload(h, false, start);
}

void ClipboardBrowser::preloadCurrentPageLater()
{
    if ( !m_timerPreload.isActive() )
        m_timerPreload.start();
}

void ClipboardBrowser::preload(int pixels, bool above, const QModelIndex &start)
{
    QElapsedTimer t;
    t.start();

    if ( m_timerUpdateSizes.isActive() )
        updateSizes();

    const int s = spacing();
    const int direction = above ? -1 : 1;
    int row = start.row();
    QModelIndex ind = start;
    int y = 0;

    const auto margins = m_sharedData->theme.margins();
    const auto rowNumberSize = m_sharedData->theme.rowNumberSize();
    const auto padding = QPoint(rowNumberSize.width() + margins.width() - s, margins.height());

    int items = 0;
    bool anyShown = false;
    for ( ; ind.isValid() && y < pixels; row += direction, ind = index(row) ) {
        if ( isRowHidden(row) )
            continue;

        const auto rect = visualRect(ind);
        const auto pos = rect.topLeft() + padding;

        if ( d.showAt(ind, pos) )
            anyShown = true;

        if (items > 0)
            y += rect.height();

        ++items;
        if (anyShown && items > 1 && t.elapsed() > 20) {
            // Preloading takes too long, preload rest of the items later.
            preloadCurrentPageLater();
            return;
        }
    }
}

void ClipboardBrowser::moveToTop(const QModelIndex &index)
{
    if ( !index.isValid() || !isLoaded() )
        return;

    const auto data = index.data(contentType::data).toMap();
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
    const auto d = h / 4;
    const auto scrollAmount =
            (h < y + d) ? (y + d - h) / 4
          : (y < d) ? -(d - y) / 4
          : 0;

    if (scrollAmount != 0) {
        QScrollBar *v = verticalScrollBar();
        v->setValue(v->value() + scrollAmount);
    } else {
        m_timerDragDropScroll.stop();
    }
}

void ClipboardBrowser::setCurrentIndex(const QModelIndex &index)
{
    // WORKAROUND: QAbstractItemView::setCurrentIndex() seems to depend on
    //             currently pressed keyboard modifiers, which is unexpected.
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
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
    const int frameLineWidth = std::ceil(2 * ratio);

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

    for (int row = first; row <= last; ++row) {
        if ( !hideFiltered(row) ) {
            const auto newIndex = index(row);
            if ( !current.isValid() )
                current = newIndex;
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
}

void ClipboardBrowser::showEvent(QShowEvent *event)
{
    if ( m.rowCount() > 0 && d.cacheOrNull(0) == nullptr )
        scrollToTop();

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
    add(data, m_dragTargetRow);
    m_dragTargetRow = -1;
}

void ClipboardBrowser::paintEvent(QPaintEvent *e)
{
    // Hide items outside viewport.
    const auto firstVisibleIndex = indexNear(0);
    if ( firstVisibleIndex.isValid() ) {
        for (int row = firstVisibleIndex.row() - 1; row >= 0; --row) {
            auto w = d.cacheOrNull(row);
            if (w)
                w->widget()->hide();
        }
    }
    const int h = viewport()->contentsRect().height();
    const auto lastVisibleIndex = indexNear(h - 3 * spacing());
    if ( lastVisibleIndex.isValid() ) {
        for (int row = lastVisibleIndex.row() + 1; row < m.rowCount(); ++row) {
            auto w = d.cacheOrNull(row);
            if (w)
                w->widget()->hide();
        }
    }

    preloadCurrentPage();

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
    qApp->removeEventFilter(this);

    if (dropAction == Qt::MoveAction) {
        selected.clear();

        for (const auto &index : indexesToRemove)
            selected.append(index);

        QWidget *target = qobject_cast<QWidget*>(drag->target());

        // Move items only if target is this app.
        if (target == this || target == viewport()) {
            moveIndexes(indexesToRemove, m_dragTargetRow, &m, MoveType::Absolute);
        } else if ( target && target->window() == window()
                    && m_itemSaver->canRemoveItems(selected, nullptr) )
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

void ClipboardBrowser::enterEvent(QEvent *event)
{
    m_ignoreMouseMoveWithButtonPressed = true;
    QListView::enterEvent(event);
}

void ClipboardBrowser::doItemsLayout()
{
    // Keep visible current item (or the first one visible)
    // on the same position visually after relayout.

    // FIXME: Virtual method QListView::doItemsLayout() is undocumented
    //        so other way should be used instead.

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
    }
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
    if ( !startEditor(editor) )
        return false;

    if (changeClipboard) {
        connect( editor, SIGNAL(fileModified(QByteArray,QString,QModelIndex)),
                 this, SLOT(onEditorNeedsChangeClipboard(QByteArray,QString)) );
    }

    return true;
}

bool ClipboardBrowser::openEditor(const QModelIndex &index)
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
        if ( data.contains(mimeText) ) {
            auto itemEditor = new ItemEditor( data[mimeText].toByteArray(), mimeText, m_sharedData->editor, this );
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

    editItem(ind, true);
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

void ClipboardBrowser::filterItems(const QRegularExpression &re)
{
    // Search in editor if open.
    if ( isInternalEditorOpen() ) {
        m_editor->search(re);
        return;
    }

    // Do nothing if same regexp was already set or both are empty (don't compare regexp options).
    const auto oldRe = d.searchExpression();
    if ( (oldRe.pattern().isEmpty() && re.pattern().isEmpty()) || oldRe == re )
        return;

    d.setSearch(re);

    // If search string is a number, highlight item in that row.
    bool filterByRowNumber = !m_sharedData->numberSearch;
    if (filterByRowNumber)
        m_filterRow = re.pattern().toInt(&filterByRowNumber);
    if (!filterByRowNumber)
        m_filterRow = -1;

    int row = 0;

    if ( re.pattern().isEmpty() ) {
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

void ClipboardBrowser::editNew(const QString &text, bool changeClipboard)
{
    if ( !isLoaded() )
        return;

    emit searchHideRequest();
    filterItems(QRegularExpression());
    if ( add(text) )
        editItem(currentIndex(), false, changeClipboard);
}

void ClipboardBrowser::keyPressEvent(QKeyEvent *event)
{
    // ignore any input if editing an item
    if ( isInternalEditorOpen() )
        return;

    // translate keys for vi mode
    if (m_sharedData->viMode && handleViKey(event, this))
        return;

    const Qt::KeyboardModifiers mods = event->modifiers();

    if (mods == Qt::AltModifier)
        return; // Handled by filter completion popup.

    const int key = event->key();

    switch (key) {
    // This fixes few issues with default navigation and item selections.
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageDown:
    case Qt::Key_PageUp:
    case Qt::Key_Home:
    case Qt::Key_End: {
        const auto current = currentIndex();
        int row = current.row();
        const int h = viewport()->contentsRect().height();

        // Preload next and previous pages so that up/down and page up/down keys scroll correctly.
        if ( !m_timerPreload.isActive() ) {
            if (key == Qt::Key_PageDown || key == Qt::Key_PageUp)
                preload(h, (key == Qt::Key_PageUp), current);
            else if (key == Qt::Key_Down || key == Qt::Key_Up)
                preload(2 * spacing(), (key == Qt::Key_Up), current);
            else if (key == Qt::Key_End)
                preload(h, true, index(length() - 1));
            scheduleDelayedItemsLayout();
            executeDelayedItemsLayout();
        }

        if (key == Qt::Key_PageDown || key == Qt::Key_PageUp) {
            event->accept();

            const int direction = (key == Qt::Key_PageDown) ? 1 : -1;

            QRect rect = visualRect(current);

            if ( rect.height() > h && direction < 0 ? rect.top() < 0 : rect.bottom() > h ) {
                // Scroll within long item.
                QScrollBar *v = verticalScrollBar();
                v->setValue( v->value() + direction * v->pageStep() );
                break;
            }

            if ( row == (direction > 0 ? m.rowCount() - 1 : 0) )
                break; // Nothing to do.

            rect = visualRect(current);

            const int fromY = direction > 0 ? qMax(0, rect.bottom()) : qMin(h, rect.y());
            const int y = fromY + direction * h;
            QModelIndex ind = indexNear(y);
            if (!ind.isValid())
                ind = index(direction > 0 ? m.rowCount() - 1 : 0);

            QRect rect2 = visualRect(ind);
            if (direction > 0 && rect2.y() > h && rect2.bottom() - rect.bottom() > h && row + 1 < ind.row())
                row = ind.row() - 1;
            else if (direction < 0 && rect2.bottom() < 0 && rect.y() - rect2.y() > h && row - 1 > ind.row())
                row = ind.row() + 1;
            else
                row = direction > 0 ? qMax(current.row() + 1, ind.row()) : qMin(current.row() - 1, ind.row());
        } else {
            if (key == Qt::Key_Up) {
                --row;
            } else if (key == Qt::Key_Down) {
                ++row;
            } else {
                int direction;
                if (key == Qt::Key_End) {
                    row = model()->rowCount() - 1;
                    direction = 1;
                } else {
                    row = 0;
                    direction = -1;
                }

                for ( ; row != current.row() && hideFiltered(row); row -= direction ) {}
            }
        }

        const QItemSelectionModel::SelectionFlags flags = selectionCommand(index(row), event);
        const bool setCurrentOnly = flags.testFlag(QItemSelectionModel::NoUpdate);
        const bool keepSelection = setCurrentOnly || flags.testFlag(QItemSelectionModel::SelectCurrent);

        // Use more stable page scrolling by keeping current item at top.
        if (key == Qt::Key_PageDown || key == Qt::Key_PageUp)
            scrollTo( index(row), QAbstractItemView::PositionAtTop );

        setCurrent(row, keepSelection, setCurrentOnly);
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

void ClipboardBrowser::setCurrent(int row, bool keepSelection, bool setCurrentOnly)
{
    if ( m.rowCount() == 0 )
        return;

    QModelIndex prev = currentIndex();
    int cur = prev.row();

    const int direction = cur <= row ? 1 : -1;

    // select first visible
    int i = std::max(0, std::min(row, m.rowCount() - 1));
    cur = i;
    while ( 0 <= i && i < length() && isRowHidden(i) ) {
        i = std::max(0, std::min(i + direction, m.rowCount() - 1));
        if ( i == 0 || i == m.rowCount() - 1 || i == cur)
            break;
    }
    if ( i < 0 || i >= length() || isRowHidden(i) )
        return;

    if (keepSelection) {
        auto sel = selectionModel();
        const bool currentSelected = sel->isSelected(prev);
        for ( int j = prev.row(); j != i + direction; j += direction ) {
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
        setCurrentIndex( index(i) );
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
    if ( !isLoaded() ) {
        loadItems();
        if ( !isLoaded() ) {
            log( QString("Cannot add new items. Tab %1 is not loaded.").arg(m_tabName), LogWarning );
            return false;
        }
    }

    const int newRow = row < 0 ? m.rowCount() : qMin(row, m.rowCount());

    if ( data.contains(mimeItems) ) {
        const QByteArray bytes = data[mimeItems].toByteArray();
        QDataStream stream(bytes);

        QList<QVariantMap> dataList;
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
        const QVariantMap previousData = copyIndex(firstIndex);

        if ( firstIndex.isValid()
             && previousData.contains(mimeText)
             // Don't update edited item.
             && (!isInternalEditorOpen() || currentIndex() != firstIndex) )
        {
            const auto newText = getTextData(data);
            const auto oldText = getTextData(previousData);
            if ( (mode == ClipboardMode::Clipboard)
                 ? (newText == oldText)
                 : newText.contains(oldText) )
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

void ClipboardBrowser::editRow(int row)
{
    editItem( index(row) );
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
    const auto data = itemData(index);
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

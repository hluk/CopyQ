// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemdelegate.h"

#include "common/client_server.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/sanitize_text_document.h"
#include "common/textdata.h"
#include "common/timer.h"
#include "gui/clipboardbrowser.h"
#include "gui/iconfactory.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/itemeditorwidget.h"
#include "item/persistentdisplayitem.h"

#include <QEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const char propertySelectedItem[] = "CopyQ_selected";
constexpr int defaultMaxItemHeight = 2048 * 8;

} // namespace

ItemDelegate::ItemDelegate(ClipboardBrowser *view, const ClipboardBrowserSharedPtr &sharedData, QWidget *parent)
    : QItemDelegate(parent)
    , m_view(view)
    , m_sharedData(sharedData)
    , m_maxWidth(2048)
    , m_idealWidth(m_view->viewport()->contentsRect().width())
{
    initSingleShotTimer(
        &m_timerInvalidateHidden, 0, this, &ItemDelegate::invalidateAllHiddenNow );
}

ItemDelegate::~ItemDelegate() = default;

QSize ItemDelegate::sizeHint(const QModelIndex &index) const
{
    return m_items[index.row()].size;
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &,
                             const QModelIndex &index) const
{
    return sizeHint(index);
}

bool ItemDelegate::eventFilter(QObject *obj, QEvent *event)
{
    // resize event for items
    if ( event->type() == QEvent::Resize ) {
        const int row = findWidgetRow(obj);
        Q_ASSERT(row != -1);
        const auto index = m_view->index(row);
        updateLater();
        const auto ev = static_cast<QResizeEvent*>(event);
        updateItemSize(index, ev->size());
    }

    return QItemDelegate::eventFilter(obj, event);
}

void ItemDelegate::dataChanged(const QModelIndex &a, const QModelIndex &b)
{
    for ( int row = a.row(); row <= b.row(); ++row )
        m_items[row].item.reset();

    updateLater();
}

void ItemDelegate::rowsRemoved(const QModelIndex &, int start, int end)
{
    for (int row = start; row <= end; ++row) {
        if ( m_view->isRowHidden(row) )
            continue;

        if (m_items[row]) {
            const auto index = m_view->index(row);
            m_items[row]->widget()->removeEventFilter(this);
            setIndexWidget(index, nullptr);
        }
    }

    m_items.erase(std::begin(m_items) + start, std::begin(m_items) + end + 1);

    updateLater();
}

void ItemDelegate::rowsMoved(const QModelIndex &, int sourceStart, int sourceEnd,
                             const QModelIndex &, int destinationRow)
{
    auto count = sourceEnd - sourceStart + 1;
    auto from = sourceStart;
    auto to = destinationRow;

    if (to < from) {
        std::swap(from, to);
        to += count;
        count = to - from - count;
    }

    const auto start1 = std::begin(m_items) + from;
    const auto start2 = start1 + count;
    const auto end2 = std::begin(m_items) + to;
    std::rotate(start1, start2, end2);

    updateLater();
}

QWidget *ItemDelegate::createPreview(const QVariantMap &data, QWidget *parent)
{
    const bool antialiasing = m_sharedData->theme.isAntialiasingEnabled();
    ItemWidget *itemWidget =
            m_sharedData->itemFactory->createItem(data, parent, antialiasing, false, true);

    const auto height = 2048 * 8;
    const auto idealWidth = parent->contentsRect().width();
    const auto maxSize = m_sharedData->textWrap ? QSize(idealWidth, height) : QSize(2048, height);
    itemWidget->updateSize(maxSize, idealWidth);

    highlightMatches(itemWidget);

    parent->setFocusProxy( itemWidget->widget() );

    emit itemWidgetCreated(PersistentDisplayItem(this, data, itemWidget->widget()));

    return itemWidget->widget();
}

void ItemDelegate::rowsInserted(const QModelIndex &, int start, int end)
{
    const auto count = static_cast<size_t>(end - start + 1);
    const auto oldSize = m_items.size();
    m_items.resize(oldSize + count);
    std::rotate( std::begin(m_items) + start,
                 std::begin(m_items) + oldSize,
                 std::end(m_items) );

    updateLater();
}

void ItemDelegate::createItemWidget(const QModelIndex &index)
{
    const int row = index.row();
    ItemWidget *w = m_items[row].get();
    if (w == nullptr) {
        QVariantMap data = index.data(contentType::data).toMap();
        data.insert(mimeCurrentTab, m_view->tabName());
        w = updateWidget(index, data);
        emit itemWidgetCreated(PersistentDisplayItem(this, data, w->widget()));
    }
}

void ItemDelegate::updateWidget(QObject *widget, const QVariantMap &data)
{
    if ( widget->parent() != m_view->viewport() ) {
        auto previewParent = qobject_cast<QWidget*>( widget->parent() );
        Q_ASSERT(previewParent);
        if (!previewParent)
            return;

        auto scrollArea = qobject_cast<QScrollArea*>( previewParent->parentWidget() );
        Q_ASSERT(scrollArea);
        if (!scrollArea)
            return;

        auto newPreview = createPreview(data, scrollArea);
        scrollArea->setWidget(newPreview);
        newPreview->show();
        return;
    }

    const auto row = findWidgetRow(widget);
    if (row == -1)
        return;

    const auto index = m_view->index(row);
    updateWidget(index, data);
}

void ItemDelegate::setItemSizes(int maxWidth, int idealWidth)
{
    const auto margins = m_sharedData->theme.margins();
    const int margin = 2 * margins.width() + 2 * m_view->spacing();
    m_maxWidth = maxWidth - margin;
    m_idealWidth = idealWidth - margin;

    for (int row = 0; static_cast<size_t>(row) < m_items.size(); ++row) {
        if (m_items[row])
            updateItemWidgetSize(row);
    }
}

void ItemDelegate::updateItemWidgetSize(int row)
{
    const int rowNumberWidth = m_sharedData->theme.rowNumberSize(row).width();
    ItemWidget *w = m_items[row].get();
    w->updateSize(
        QSize(m_maxWidth - rowNumberWidth, defaultMaxItemHeight),
        m_idealWidth - rowNumberWidth
    );
}

void ItemDelegate::updateItemSize(const QModelIndex &index, QSize itemWidgetSize)
{
    const int row = index.row();
    const auto margins = m_sharedData->theme.margins();
    const auto rowNumberSize = m_sharedData->theme.rowNumberSize(row);
    const int width = itemWidgetSize.width() + 2 * margins.width() + rowNumberSize.width();
    const int height = std::max(
        itemWidgetSize.height() + 2 * margins.height(),
        rowNumberSize.height()
    );

    const QSize newSize = QSize(width, height);
    if (m_items[row].size == newSize)
        return;

    m_items[row].size = newSize;
    emit sizeHintChanged(index);
}

ItemEditorWidget *ItemDelegate::createCustomEditor(
        QWidget *parent, const QModelIndex &index, const QString &format)
{
    // Refuse editing non-text data
    if ( format != mimeItemNotes && !format.startsWith(QLatin1String("text/")) )
        return nullptr;

    const QVariantMap data = m_sharedData->itemFactory->data(index);
    if ( format != mimeItemNotes && !data.contains(format) )
        return nullptr;

    auto editorParent = new QWidget(parent);
    auto editor = new ItemEditorWidget(index, format, editorParent);

    connect(editor, &QObject::destroyed, editorParent, &QObject::deleteLater);

    // Prefer editing rich text
    if ( format == mimeText && data.contains(mimeHtml) ) {
        const QString html = getTextData(data, mimeHtml);
        editor->setHtml(html);
        sanitizeTextDocument(editor->document());
    } else {
        const QString text = getTextData(data, format);
        editor->setPlainText(text);
    }

    editor->selectAll();

    auto toolBar = editor->createToolbar(editorParent, m_sharedData->menuItems);
    auto palette = m_sharedData->theme.editorPalette();
    editorParent->setBackgroundRole(QPalette::Base);
    editorParent->setAutoFillBackground(true);
    editorParent->setPalette(palette);
    editor->setPalette(palette);
    editor->setStyleSheet("QTextEdit{background:transparent}");

    palette.setColor(QPalette::Base, Qt::transparent);
    editor->setPalette(palette);

    const auto font = m_sharedData->theme.editorFont();
    editorParent->setFont(font);
    editor->setFont(font);
    toolBar->setFont(font);

    editor->setSaveOnReturnKey(m_sharedData->saveOnReturnKey);

    auto layout = new QVBoxLayout(editorParent);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(toolBar);
    layout->addWidget(editor);

    editorParent->show();

    return editor;
}

void ItemDelegate::highlightMatches(ItemWidget *itemWidget) const
{
    QWidget *w = itemWidget->widget();
    const auto textEdits = w->findChildren<QTextEdit*>();
    auto maybeTextEdit = qobject_cast<QTextEdit*>(w);
    if (m_filter) {
        QTextCharFormat format;
        format.setFont( m_sharedData->theme.searchFont() );
        QPalette palette = m_sharedData->theme.searchPalette();
        format.setBackground( palette.base() );
        format.setForeground( palette.text() );

        for (QTextEdit *edit : textEdits)
            m_filter->highlight(edit, format);
        if (maybeTextEdit)
            m_filter->highlight(maybeTextEdit, format);
    } else {
        for (QTextEdit *edit : textEdits)
            edit->setExtraSelections({});
        if (maybeTextEdit)
            maybeTextEdit->setExtraSelections({});
    }
}

void ItemDelegate::updateAllRows()
{
    const int s = m_view->spacing();
    const int space = 2 * s;
    int y = -m_view->verticalOffset() + s;

    for (int row = 0; static_cast<size_t>(row) < m_items.size(); ++row) {
        const bool hide = m_view->isRowHidden(row);
        auto &item = m_items[row];
        if (item) {
            QWidget *ww = item->widget();
            if (hide) {
                ww->removeEventFilter(this);
                ww->hide();
            } else {
                if (item.appliedFilterId != m_filterId) {
                    highlightMatches(item.get());
                    item.appliedFilterId = m_filterId;
                }
                ww->move( QPoint(ww->x(), y) );
                if ( ww->isHidden() ) {
                    ww->show();
                    updateItemWidgetSize(row);
                    ww->installEventFilter(this);
                    const auto index = m_view->index(row);
                    updateItemSize(index, ww->size());
                }
            }
        }

        if (!hide)
            y += m_items[row].size.height() + space;
    }
}

void ItemDelegate::updateLater()
{
    m_timerInvalidateHidden.start();
}

void ItemDelegate::updateIfNeeded()
{
    if ( m_timerInvalidateHidden.isActive() )
        invalidateAllHiddenNow();
}

void ItemDelegate::setItemWidgetCurrent(const QModelIndex &index, bool isCurrent)
{
    ItemWidget *w;

    const int row = index.row();

    if (isCurrent) {
        createItemWidget(index);
        w = m_items[row].get();
        auto ww = w->widget();
        QPalette palette( ww->palette() );
        const auto highlightPalette = m_sharedData->theme.searchPalette();
        palette.setColor(QPalette::Highlight, highlightPalette.base().color());
        palette.setColor(QPalette::HighlightedText, highlightPalette.text().color());
        ww->setPalette(palette);
        for ( auto childWidget : ww->findChildren<QWidget*>() )
            childWidget->setPalette(palette);
    } else {
        w = m_items[row].get();
        if (!w)
            return;
    }

    w->setCurrent(isCurrent);
}

void ItemDelegate::setItemWidgetSelected(const QModelIndex &index, bool isSelected)
{
    const int row = index.row();
    auto w = m_items[row].get();
    if (!w)
        return;

    auto ww = w->widget();
    setWidgetSelected(ww, isSelected);
}

void ItemDelegate::setIndexWidget(const QModelIndex &index, ItemWidget *w)
{
    const int row = index.row();
    const QPoint pos = w ? findPositionForWidget(index) : QPoint();
    const bool show = w && !m_view->isIndexHidden(index);

    auto &item = m_items[row];
    item.item.reset(w);
    item.appliedFilterId = 0;
    if (w) {
        QWidget *ww = w->widget();

        // Make background transparent.
        ww->setAttribute(Qt::WA_NoSystemBackground);

        const bool isCurrent = m_view->currentIndex() == index;
        setItemWidgetCurrent(index, isCurrent);

        const bool isSelected = m_view->selectionModel()->isSelected(index);
        setWidgetSelected(ww, isSelected);

        ww->move(pos);

        if (show) {
            ww->show();
            updateItemWidgetSize(row);
        }

        ww->installEventFilter(this);
        updateItemSize(index, ww->size());
        updateLater();
    }
}

QPoint ItemDelegate::findPositionForWidget(const QModelIndex &index) const
{
    const QSize margins = m_sharedData->theme.margins();
    const QSize rowNumberSize = m_sharedData->theme.rowNumberSize(index.row());
    const int s = m_view->spacing();

    int y = 0;
    int skipped = 0;
    for (int row = index.row() - 1; row >= 0; --row) {
        if (!m_items[row]) {
            if ( !m_view->isRowHidden(row) )
                ++skipped;
            continue;
        }

        QWidget *ww = m_items[row]->widget();
        if ( ww->isHidden() )
            continue;

        y = ww->geometry().top() - margins.height() + m_items[row].size.height()
            + skipped * (defaultItemHeight + 2 * s);
        break;
    }

    return QPoint(
        s + margins.width() + rowNumberSize.width(),
        s + margins.height() + y
    );
}

void ItemDelegate::setCurrentRow(int row, bool current)
{
    ItemWidget *item = m_items[row].get();
    if (!item)
        return;

    item->setCurrent(false);
    item->setCurrent(current);
}

void ItemDelegate::setWidgetSelected(QWidget *ww, bool selected)
{
    if ( ww->property(propertySelectedItem).toBool() == selected )
        return;

    ww->setProperty(propertySelectedItem, selected);

    QStyle *style = m_view->style();
    ww->setStyle(style);
    for (auto child : ww->findChildren<QWidget *>())
        child->setStyle(style);
    ww->update();
}

int ItemDelegate::findWidgetRow(const QObject *obj) const
{
    for (int row = 0; static_cast<size_t>(row) < m_items.size(); ++row) {
        auto w = m_items[row].get();
        if (w && w->widget() == obj)
            return row;
    }

    return -1;
}

ItemWidget *ItemDelegate::updateWidget(const QModelIndex &index, const QVariantMap &data)
{
    const bool antialiasing = m_sharedData->theme.isAntialiasingEnabled();
    QWidget *parent = m_view->viewport();

    auto w = m_sharedData->showSimpleItems
            ? m_sharedData->itemFactory->createSimpleItem(data, parent, antialiasing)
            : m_sharedData->itemFactory->createItem(data, parent, antialiasing);

    setIndexWidget(index, w);

    return w;
}

bool ItemDelegate::invalidateHidden(QWidget *widget)
{
    if ( widget->isVisible() && m_view->isVisible() )
        return false;

    const auto row = findWidgetRow(widget);
    if (row == -1)
        return true;

    const auto index = m_view->index(row);
    if ( m_view->currentIndex() == index )
        return false;

    setIndexWidget(index, nullptr);
    return true;
}

void ItemDelegate::invalidateAllHiddenNow()
{
    m_timerInvalidateHidden.stop();

    // Make sure item widgets have up-to-date positions for invalidation.
    updateAllRows();

    const QRect viewRect = m_view->viewport()->contentsRect();
    const int maxY = viewRect.bottom() + viewRect.height() + defaultItemHeight;
    const int minY = viewRect.top() - viewRect.height() - defaultItemHeight;
    const int currentRow = m_view->currentIndex().row();
    const auto isRowAlmostVisible = [&](int row){
        if (row < 0 || static_cast<size_t>(row) >= m_items.size() || !m_items[row])
            return false;

        QWidget *ww = m_items[row]->widget();
        if ( ww->isHidden() )
            return false;

        const QRect g = ww->geometry();
        return minY < g.bottom() && g.top() < maxY;
    };

    for (int row = 0; static_cast<size_t>(row) < m_items.size(); ++row) {
        if (!m_items[row] || row == currentRow)
            continue;

        if ( isRowAlmostVisible(row) )
            continue;

        if ( isRowAlmostVisible(row + 1) || isRowAlmostVisible(row - 1) )
            continue;

        const auto index = m_view->index(row);
        m_items[row]->widget()->removeEventFilter(this);
        setIndexWidget(index, nullptr);
    }
}

void ItemDelegate::setItemFilter(const ItemFilterPtr &filter)
{
    m_filter = filter;
    ++m_filterId;
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const bool isSelected = option.state & QStyle::State_Selected;

    // Render background (selected, alternate, ...).
    QStyle *style = m_view->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, m_view);

    // Colorize item.
    const QString colorExpr = index.data(contentType::color).toString();
    if (!colorExpr.isEmpty()) {
        const QColor color = m_sharedData->theme.evalColorExpression(colorExpr);
        if (color.isValid()) {
            const auto oldMode = painter->compositionMode();
            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
            painter->fillRect(option.rect, color);
            painter->setCompositionMode(oldMode);
        }
    }

    const auto margins = m_sharedData->theme.margins();

    // Render number.
    if ( m_sharedData->theme.showRowNumber() ) {
        const int rowNumber = index.row() + static_cast<int>(m_sharedData->rowIndexFromOne);
        const QString num = QString::number(rowNumber);
        QPalette::ColorRole role = isSelected ? QPalette::HighlightedText : QPalette::Text;
        painter->save();
        painter->setFont( m_sharedData->theme.rowNumberFont() );
        const auto rowNumberPalette = m_sharedData->theme.rowNumberPalette();
        style->drawItemText(painter, option.rect.translated(margins.width(), margins.height()),
                            Qt::AlignTop | Qt::AlignLeft,
                            rowNumberPalette, true, num,
                            role);
        painter->restore();
    }
}

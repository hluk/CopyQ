/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include "itemdelegate.h"

#include "common/client_server.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "gui/clipboardbrowser.h"
#include "gui/iconfactory.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/itemeditorwidget.h"
#include "item/persistentdisplayitem.h"

#include <QEvent>
#include <QPainter>

#include <algorithm>

namespace {

const char propertySelectedItem[] = "CopyQ_selected";

} // namespace

ItemDelegate::ItemDelegate(ClipboardBrowser *view, const ClipboardBrowserSharedPtr &sharedData, QWidget *parent)
    : QItemDelegate(parent)
    , m_view(view)
    , m_sharedData(sharedData)
    , m_saveOnReturnKey(true)
    , m_re()
    , m_maxSize(2048, 2048 * 8)
    , m_idealWidth(0)
    , m_cache()
{
}

ItemDelegate::~ItemDelegate() = default;

QSize ItemDelegate::sizeHint(const QModelIndex &index) const
{
    const int row = index.row();
    if ( static_cast<size_t>(row) < m_cache.size() ) {
        const ItemWidget *w = cacheOrNull(row);
        if (w != nullptr) {
            QWidget *ww = w->widget();
            const auto margins = m_sharedData->theme.margins();
            const auto rowNumberSize = m_sharedData->theme.rowNumberSize();
            return QSize( ww->width() + 2 * margins.width() + rowNumberSize.width(),
                          qMax(ww->height() + 2 * margins.height(), rowNumberSize.height()) );
        }
    }
    return QSize(0, 100);
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

        const auto index = m_view->model()->index(row, 0);
        if ( index.isValid() )
            emit sizeHintChanged(index);
    }

    return QItemDelegate::eventFilter(obj, event);
}

void ItemDelegate::dataChanged(const QModelIndex &a, const QModelIndex &b)
{
    for ( int row = a.row(); row <= b.row(); ++row ) {
        auto item = &m_cache[row];
        if (*item) {
            item->reset();
            cache( m_view->index(row) );
        }
    }
}

void ItemDelegate::rowsRemoved(const QModelIndex &, int start, int end)
{
    m_cache.erase(std::begin(m_cache) + start, std::begin(m_cache) + end + 1);
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

    const auto start1 = std::begin(m_cache) + from;
    const auto start2 = start1 + count;
    const auto end2 = std::begin(m_cache) + to;
    std::rotate(start1, start2, end2);
}

void ItemDelegate::rowsInserted(const QModelIndex &, int start, int end)
{
    const auto count = static_cast<size_t>(end - start + 1);
    const auto oldSize = m_cache.size();
    m_cache.resize(oldSize + count);
    std::rotate( std::begin(m_cache) + start,
                 std::begin(m_cache) + oldSize,
                 std::end(m_cache) );
}

ItemWidget *ItemDelegate::cache(const QModelIndex &index)
{
    const int row = index.row();
    ItemWidget *w = cacheOrNull(row);
    if (w == nullptr) {
        auto data = m_view->itemData(index);
        data.insert(mimeCurrentTab, m_view->tabName());
        w = updateCache(index, data);
        emit itemWidgetCreated(PersistentDisplayItem(this, data, w->widget()));
    }

    return w;
}

void ItemDelegate::updateCache(QObject *widget, const QVariantMap &data)
{
    const auto row = findWidgetRow(widget);
    if (row == -1)
        return;

    const auto index = m_view->index(row);
    updateCache(index, data);
}

ItemWidget *ItemDelegate::cacheOrNull(int row) const
{
    return m_cache[static_cast<size_t>(row)].get();
}

bool ItemDelegate::hasCache(const QModelIndex &index) const
{
    const int row = index.row();
    return cacheOrNull(row) != nullptr;
}

void ItemDelegate::setItemSizes(QSize size, int idealWidth)
{
    const auto margins = m_sharedData->theme.margins();
    const auto rowNumberSize = m_sharedData->theme.rowNumberSize();
    const int margin = 2 * margins.width() + rowNumberSize.width();
    m_maxSize.setWidth(size.width() - margin);
    m_idealWidth = idealWidth - margin;

    if (m_idealWidth > 0) {
        for (auto &w : m_cache) {
            if (w != nullptr)
                w->updateSize(m_maxSize, m_idealWidth);
        }
    }
}

bool ItemDelegate::otherItemLoader(const QModelIndex &index, bool next)
{
    const int row = index.row();
    ItemWidget *w = cacheOrNull(row);
    if (w != nullptr) {
        const auto data = m_view->itemData(index);
        const bool antialiasing = m_sharedData->theme.isAntialiasingEnabled();
        auto w2 = m_sharedData->itemFactory->otherItemLoader(data, w, next, antialiasing);
        if (w2 != nullptr) {
            setIndexWidget(index, w2);
            return true;
        }
    }

    return false;
}

ItemEditorWidget *ItemDelegate::createCustomEditor(
        QWidget *parent, const QModelIndex &index, bool editNotes)
{
    const auto w = cache(index);
    auto editor = new ItemEditorWidget(w, index, editNotes, parent);
    editor->setEditorPalette( m_sharedData->theme.editorPalette() );
    editor->setEditorFont( m_sharedData->theme.editorFont() );
    editor->setSaveOnReturnKey(m_saveOnReturnKey);
    return editor;
}

void ItemDelegate::highlightMatches(ItemWidget *itemWidget) const
{
    const auto &font = m_sharedData->theme.searchFont();
    const auto &palette = m_sharedData->theme.searchPalette();
    itemWidget->setHighlight(m_re, font, palette);
}

void ItemDelegate::setItemWidgetCurrent(const QModelIndex &index, bool isCurrent)
{
    ItemWidget *w;
    if (isCurrent) {
        w = cache(index);

        auto ww = w->widget();
        QPalette palette( ww->palette() );
        const auto highlightPalette = m_sharedData->theme.searchPalette();
        palette.setColor(QPalette::Highlight, highlightPalette.base().color());
        palette.setColor(QPalette::HighlightedText, highlightPalette.text().color());
        ww->setPalette(palette);
        for ( auto childWidget : ww->findChildren<QWidget*>() )
            childWidget->setPalette(palette);
    } else {
        const int row = index.row();
        w = cacheOrNull(row);
        if (!w)
            return;
    }

    w->setCurrent(isCurrent);
    if (m_idealWidth > 0)
        w->updateSize(m_maxSize, m_idealWidth);
}

void ItemDelegate::setItemWidgetSelected(const QModelIndex &index, bool isSelected)
{
    const int row = index.row();
    auto w = cacheOrNull(row);
    if (!w)
        return;

    auto ww = w->widget();
    setWidgetSelected(ww, isSelected);
}

void ItemDelegate::setIndexWidget(const QModelIndex &index, ItemWidget *w)
{
    const int row = index.row();
    m_cache[row].reset(w);
    if (w == nullptr)
        return;

    QWidget *ww = w->widget();

    // Make background transparent.
    ww->setAttribute(Qt::WA_NoSystemBackground);

    const bool isCurrent = m_view->currentIndex() == index;
    setItemWidgetCurrent(index, isCurrent);

    const bool isSelected = m_view->selectionModel()->isSelected(index);
    setWidgetSelected(ww, isSelected);

    // Try to get proper size by showing item momentarily.
    if (m_idealWidth > 0) {
        ww->show();
        w->updateSize(m_maxSize, m_idealWidth);
        ww->hide();
    }

    ww->installEventFilter(this);

    // TODO: Check if sizeHint() really changes.
    emit sizeHintChanged(index);
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
    for (int row = 0; static_cast<size_t>(row) < m_cache.size(); ++row) {
        auto w = cacheOrNull(row);
        if (w && w->widget() == obj)
            return row;
    }

    return -1;
}

ItemWidget *ItemDelegate::updateCache(const QModelIndex &index, const QVariantMap &data)
{
    const bool antialiasing = m_sharedData->theme.isAntialiasingEnabled();
    QWidget *parent = m_view->viewport();

    auto w = m_sharedData->showSimpleItems
            ? m_sharedData->itemFactory->createSimpleItem(data, parent, antialiasing)
            : m_sharedData->itemFactory->createItem(data, parent, antialiasing);

    setIndexWidget(index, w);
    return w;
}

void ItemDelegate::invalidateCache()
{
    for(auto &w : m_cache)
        w.reset();
}

bool ItemDelegate::invalidateHidden(QWidget *widget)
{
    if ( widget->isVisible() && m_view->isVisible() )
        return false;

    const auto row = findWidgetRow(widget);
    Q_ASSERT(row != -1);
    const auto index = m_view->index(row);
    setIndexWidget(index, nullptr);
    return true;
}

void ItemDelegate::setSearch(const QRegExp &re)
{
    m_re = re;
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const QRect &rect = option.rect;

    const bool isSelected = option.state & QStyle::State_Selected;

    // Render background (selected, alternate, ...).
    QStyle *style = m_view->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, m_view);

    const int row = index.row();
    const auto w = cacheOrNull(row);
    if (w == nullptr) {
        m_view->updateItemWidget(index);
        return;
    }

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
        const QString num = QString::number(row);
        QPalette::ColorRole role = isSelected ? QPalette::HighlightedText : QPalette::Text;
        painter->save();
        painter->setFont( m_sharedData->theme.rowNumberFont() );
        const auto rowNumberPalette = m_sharedData->theme.rowNumberPalette();
        style->drawItemText(painter, rect.translated(margins.width() / 2, margins.height()), 0,
                            rowNumberPalette, true, num,
                            role);
        painter->restore();
    }

    highlightMatches(w);

    auto ww = w->widget();
    const auto rowNumberSize = m_sharedData->theme.rowNumberSize();
    const auto offset = rect.topLeft() + QPoint(rowNumberSize.width() + margins.width(), margins.height());
    ww->move(offset);
    if ( ww->isHidden() ) {
        ww->show();
        if (m_idealWidth > 0)
            w->updateSize(m_maxSize, m_idealWidth);

        // Workaround: This fixes badly rendered items.
        ww->update();
    }
}

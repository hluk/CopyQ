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

#include "itemdelegate.h"

#include "common/client_server.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "gui/clipboardbrowser.h"
#include "gui/iconfactory.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/itemeditorwidget.h"

#include <QEvent>
#include <QPainter>

#include <algorithm>

namespace {

const char propertySelectedItem[] = "CopyQ_selected";

inline void reset(ItemWidget **ptr, ItemWidget *value = nullptr)
{
    delete *ptr;
    *ptr = value != nullptr ? value : nullptr;
}

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
    int row = index.row();
    if ( row < m_cache.size() ) {
        const ItemWidget *w = m_cache[row];
        if (w != nullptr) {
            QWidget *ww = w->widget();
            const auto margins = m_sharedData->theme.margins();
            const auto rowNumberSize = m_sharedData->theme.rowNumberSize();
            return QSize( ww->width() + 2 * margins.width() + rowNumberSize.width(),
                          qMax(ww->height() + 2 * margins.height(), rowNumberSize.height()) );
        }
    }
    return QSize(0, 512);
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
        int row = 0;
        for (; row < m_cache.size(); ++row) {
            auto w = m_cache[row];
            if (w && w->widget() == obj)
                break;
        }

        Q_ASSERT( row < m_cache.size() );

        const auto index = m_view->model()->index(row, 0);
        if ( index.isValid() )
            emit sizeHintChanged(index);
    }

    return false;
}

void ItemDelegate::dataChanged(const QModelIndex &a, const QModelIndex &b)
{
    for ( int row = a.row(); row <= b.row(); ++row ) {
        auto item = &m_cache[row];
        if (*item) {
            reset(item);
            cache( m_view->index(row) );
        }
    }
}

void ItemDelegate::rowsRemoved(const QModelIndex &, int start, int end)
{
    for( int i = end; i >= start; --i ) {
        delete m_cache.takeAt(i);
    }
}

void ItemDelegate::rowsMoved(const QModelIndex &, int sourceStart, int sourceEnd,
                             const QModelIndex &, int destinationRow)
{
    int dest = sourceStart < destinationRow ? destinationRow-1 : destinationRow;
    for( int i = sourceStart; i <= sourceEnd; ++i ) {
        m_cache.move(i,dest);
        ++dest;
    }
}

void ItemDelegate::rowsInserted(const QModelIndex &, int start, int end)
{
    for( int i = start; i <= end; ++i )
        m_cache.insert(i, nullptr);
}

ItemWidget *ItemDelegate::cache(const QModelIndex &index)
{
    int n = index.row();

    ItemWidget *w = m_cache[n];
    if (w == nullptr) {
        QWidget *parent = m_view->viewport();
        const bool antialiasing = m_sharedData->theme.isAntialiasingEnabled();
        w = m_sharedData->showSimpleItems
                ? m_sharedData->itemFactory->createSimpleItem(index, parent, antialiasing)
                : m_sharedData->itemFactory->createItem(index, parent, antialiasing);
        setIndexWidget(index, w);
    }

    return w;
}

bool ItemDelegate::hasCache(const QModelIndex &index) const
{
    return m_cache[index.row()] != nullptr;
}

void ItemDelegate::setItemSizes(const QSize &size, int idealWidth)
{
    const auto margins = m_sharedData->theme.margins();
    const auto rowNumberSize = m_sharedData->theme.rowNumberSize();
    const int margin = 2 * margins.width() + rowNumberSize.width();
    m_maxSize.setWidth(size.width() - margin);
    m_idealWidth = idealWidth - margin;

    for(auto w : m_cache) {
        if (w != nullptr)
            w->updateSize(m_maxSize, m_idealWidth);
    }
}

void ItemDelegate::setRowVisible(int row, bool visible)
{
    ItemWidget *w = m_cache[row];
    if (w != nullptr) {
        if (visible)
            highlightMatches(w);
    }
}

bool ItemDelegate::otherItemLoader(const QModelIndex &index, bool next)
{
    ItemWidget *w = m_cache[index.row()];
    if (w != nullptr) {
        const bool antialiasing = m_sharedData->theme.isAntialiasingEnabled();
        auto w2 = m_sharedData->itemFactory->otherItemLoader(index, w, next, antialiasing);
        if (w2 != nullptr) {
            setIndexWidget(index, w2);
            return true;
        }
    }

    return false;
}

ItemEditorWidget *ItemDelegate::createCustomEditor(QWidget *parent, const QModelIndex &index,
                                                   bool editNotes)
{
    cache(index);
    auto editor = new ItemEditorWidget(m_cache[index.row()], index, editNotes, parent);
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

void ItemDelegate::setItemWidgetStatic(const QModelIndex &index, bool isStatic)
{
    if (isStatic) {
        auto w = m_cache[index.row()];
        if (w)
            w->widget()->hide();
    } else {
        const auto rect = m_view->visualRect(index);
        const auto position = rect.topLeft();
        const auto margins = m_sharedData->theme.margins();
        const auto rowNumberSize = m_sharedData->theme.rowNumberSize();
        const auto offset = QPoint(rowNumberSize.width() + margins.width(), margins.height());
        auto w = cache(index);
        w->widget()->move(position + offset);
        w->widget()->show();
    }
}

void ItemDelegate::setIndexWidget(const QModelIndex &index, ItemWidget *w)
{
    reset(&m_cache[index.row()], w);
    if (w == nullptr)
        return;

    QWidget *ww = w->widget();

    // Try to get proper size by showing item momentarily.
    ww->show();
    w->updateSize(m_maxSize, m_idealWidth);
    ww->hide();

    ww->installEventFilter(this);

    // TODO: Check if sizeHint() really changes.
    emit sizeHintChanged(index);
}

void ItemDelegate::invalidateCache()
{
    for(auto &w : m_cache)
        reset(&w);
}

void ItemDelegate::invalidateCache(int row)
{
    reset(&m_cache[row]);
}

void ItemDelegate::setSearch(const QRegExp &re)
{
    m_re = re;
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const int row = index.row();
    auto w = m_cache[row];
    if (w == nullptr) {
        m_view->itemWidget(index);
        return;
    }

    const QRect &rect = option.rect;

    bool isSelected = option.state & QStyle::State_Selected;

    /* render background (selected, alternate, ...) */
    QStyle *style = m_view->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, m_view);

    // Colorize item.
    const QString colorExpr = index.data(contentType::color).toString();
    if (!colorExpr.isEmpty())
    {
        const QColor color = m_sharedData->theme.evalColorExpression(colorExpr);
        if (color.isValid())
        {
            painter->save();
            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
            painter->fillRect(option.rect, color);
            painter->restore();
        }
    }

    const auto margins = m_sharedData->theme.margins();

    /* render number */
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

    /* text color for selected/unselected item */
    QWidget *ww = w->widget();
    if ( ww->property(propertySelectedItem) != isSelected ) {
        ww->setProperty(propertySelectedItem, isSelected);
        if ( !ww->property("CopyQ_no_style").toBool() ) {
            ww->setStyle(style);
            for (auto child : ww->findChildren<QWidget *>())
                child->setStyle(style);
            ww->update();
        }
    }

    highlightMatches(w);

    if ( ww->isHidden() ) {
        const auto rowNumberSize = m_sharedData->theme.rowNumberSize();
        const auto offset = rect.topLeft() + QPoint(rowNumberSize.width() + margins.width(), margins.height());
        const auto p = painter->deviceTransform().map(offset);
        ww->render(painter, p);
    }
}

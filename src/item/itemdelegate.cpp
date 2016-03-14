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

#include "itemdelegate.h"

#include "common/client_server.h"
#include "common/contenttype.h"
#include "gui/iconfactory.h"
#include "item/itemfactory.h"
#include "item/itemwidget.h"
#include "item/itemeditorwidget.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QEvent>
#include <QAbstractItemView>
#include <QPainter>

namespace {

const char propertySelectedItem[] = "CopyQ_selected";

inline void reset(ItemWidget **ptr, ItemWidget *value = NULL)
{
    delete *ptr;
    *ptr = value != NULL ? value : NULL;
}

int itemMargin()
{
    const int dpi = QApplication::desktop()->physicalDpiX();
    return ( dpi <= 120 ) ? 4 : 4 * dpi / 120;
}

} // namespace

ItemDelegate::ItemDelegate(QAbstractItemView *view, ItemFactory *itemFactory, QWidget *parent)
    : QItemDelegate(parent)
    , m_view(view)
    , m_itemFactory(itemFactory)
    , m_saveOnReturnKey(true)
    , m_re()
    , m_maxSize(2048, 2048 * 8)
    , m_idealWidth(0)
    , m_vMargin( itemMargin() )
    , m_hMargin( m_vMargin * 2 + 6 )
    , m_foundFont()
    , m_foundPalette()
    , m_rowNumberFont()
    , m_rowNumberSize(0, 0)
    , m_showRowNumber(false)
    , m_rowNumberPalette()
    , m_antialiasing(true)
    , m_cache()
{
}

ItemDelegate::~ItemDelegate()
{
}

QSize ItemDelegate::sizeHint(const QModelIndex &index) const
{
    int row = index.row();
    if ( row < m_cache.size() ) {
        const ItemWidget *w = m_cache[row];
        if (w != NULL) {
            QWidget *ww = w->widget();
            return QSize( ww->width() + 2 * m_hMargin + rowNumberWidth(),
                          qMax(ww->height() + 2 * m_vMargin, rowNumberHeight()) );
        }
    }
    return QSize(0, 512);
}

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem &,
                             const QModelIndex &index) const
{
    return sizeHint(index);
}

bool ItemDelegate::eventFilter(QObject *, QEvent *event)
{
    // resize event for items
    if ( event->type() == QEvent::Resize )
        emit rowSizeChanged();

    return false;
}

void ItemDelegate::dataChanged(const QModelIndex &a, const QModelIndex &b)
{
    // recalculating sizes of many items is expensive (when searching)
    // - assume that highlighted (matched) text has same size
    // - recalculate size only if item edited
    int row = a.row();
    if ( row == b.row() ) {
        reset(&m_cache[row]);
        emit rowSizeChanged();
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
        m_cache.insert(i, NULL);
}

ItemWidget *ItemDelegate::cache(const QModelIndex &index)
{
    int n = index.row();

    ItemWidget *w = m_cache[n];
    if (w == NULL) {
        w = m_itemFactory->createItem(index, m_view->viewport());
        setIndexWidget(index, w);
    }

    return w;
}

bool ItemDelegate::hasCache(const QModelIndex &index) const
{
    return m_cache[index.row()] != NULL;
}

void ItemDelegate::setItemSizes(const QSize &size, int idealWidth)
{
    const int margins = 2 * m_hMargin + rowNumberWidth();
    m_maxSize.setWidth(size.width() - margins);
    m_idealWidth = idealWidth - margins;

    for( int i = 0; i < m_cache.length(); ++i ) {
        ItemWidget *w = m_cache[i];
        if (w != NULL)
            w->updateSize(m_maxSize, m_idealWidth);
    }
}

void ItemDelegate::updateRowPosition(int row, int y)
{
    ItemWidget *w = m_cache[row];
    if (w != NULL)
        w->widget()->move( QPoint(rowNumberWidth() + m_hMargin, y + m_vMargin) );
}

void ItemDelegate::setRowVisible(int row, bool visible)
{
    ItemWidget *w = m_cache[row];
    if (w != NULL)
        w->widget()->setVisible(visible);
}

bool ItemDelegate::otherItemLoader(const QModelIndex &index, bool next)
{
    ItemWidget *w = m_cache[index.row()];
    if (w != NULL) {
        ItemWidget *w2 = m_itemFactory->otherItemLoader(index, w, next);
        if (w2 != NULL) {
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
    ItemEditorWidget *editor = new ItemEditorWidget(m_cache[index.row()], index, editNotes, parent);
    loadEditorSettings(editor);
    return editor;
}

void ItemDelegate::loadEditorSettings(ItemEditorWidget *editor)
{
    editor->setEditorPalette(m_editorPalette);
    editor->setEditorFont(m_editorFont);
    editor->setSaveOnReturnKey(m_saveOnReturnKey);
}

void ItemDelegate::setIndexWidget(const QModelIndex &index, ItemWidget *w)
{
    reset(&m_cache[index.row()], w);
    if (w == NULL)
        return;

    QWidget *ww = w->widget();

    if (!m_antialiasing) {
        QFont f = ww->font();
        f.setStyleStrategy(QFont::NoAntialias);
        ww->setFont(f);
        foreach (QWidget *child, ww->findChildren<QWidget *>("item_child"))
            child->setFont(f);
    }

    // Try to get proper size by showing item momentarily.
    ww->show();
    w->updateSize(m_maxSize, m_idealWidth);
    ww->hide();

    ww->installEventFilter(this);

    w->setCurrent(m_view->currentIndex() == index);

    emit rowSizeChanged();
}

int ItemDelegate::rowNumberWidth() const
{
    return m_showRowNumber ? m_rowNumberSize.width() : 0;
}

int ItemDelegate::rowNumberHeight() const
{
    return m_showRowNumber ? m_rowNumberSize.height() : 0;
}

void ItemDelegate::invalidateCache()
{
    for( int i = 0; i < m_cache.length(); ++i )
        reset(&m_cache[i]);
}

void ItemDelegate::setSearch(const QRegExp &re)
{
    m_re = re;
}

void ItemDelegate::setSearchStyle(const QFont &font, const QPalette &palette)
{
    m_foundFont = font;
    m_foundPalette = palette;
}

void ItemDelegate::setEditorStyle(const QFont &font, const QPalette &palette)
{
    m_editorFont = font;
    m_editorPalette = palette;
}

void ItemDelegate::setNumberStyle(const QFont &font, const QPalette &palette)
{
    m_rowNumberFont = font;
    m_rowNumberSize = QFontMetrics(m_rowNumberFont).boundingRect( QString("0123") ).size()
            + QSize(m_hMargin / 2, 2 * m_vMargin);
    m_rowNumberPalette = palette;
}

void ItemDelegate::setRowNumberVisibility(bool visible)
{
    m_showRowNumber = visible;
}

void ItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    int row = index.row();
    ItemWidget *w = m_cache[row];
    if (w == NULL)
        return;

    const QRect &rect = option.rect;

    bool isSelected = option.state & QStyle::State_Selected;

    /* render background (selected, alternate, ...) */
    QStyle *style = m_view->style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, m_view);

    /* render number */
    if (m_showRowNumber) {
        const QString num = QString::number(row);
        QPalette::ColorRole role = isSelected ? QPalette::HighlightedText : QPalette::Text;
        painter->save();
        painter->setFont(m_rowNumberFont);
        style->drawItemText(painter, rect.translated(m_hMargin / 2, m_vMargin), 0,
                            m_rowNumberPalette, true, num,
                            role);
        painter->restore();
    }

    /* highlight search string */
    w->setHighlight(m_re, m_foundFont, m_foundPalette);

    /* text color for selected/unselected item */
    QWidget *ww = w->widget();
    if ( ww->property(propertySelectedItem) != isSelected ) {
        ww->setProperty(propertySelectedItem, isSelected);
        if ( !ww->property("CopyQ_no_style").toBool() ) {
            ww->setStyle(style);
            foreach (QWidget *child, ww->findChildren<QWidget *>())
                child->setStyle(style);
            ww->update();
        }
    }
}

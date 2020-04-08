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

#ifndef ITEMDELEGATE_H
#define ITEMDELEGATE_H

#include "gui/clipboardbrowsershared.h"

#include <QItemDelegate>
#include <QRegularExpression>

#include <memory>
#include <vector>

class Item;
class ItemEditorWidget;
class ItemFactory;
class ItemWidget;
class ClipboardBrowser;
class PersistentDisplayItem;

/**
 * Delegate for items in ClipboardBrowser.
 *
 * Creates editor on demand and draws contents of all items.
 *
 * To achieve better performance the first call to get sizeHint() value for
 * an item returns some default value (so it doesn't have to render all items).
 *
 * Before calling paint() for an index item on given index must be cached
 * using cache().
 */
class ItemDelegate final : public QItemDelegate
{
    Q_OBJECT

    public:
        ItemDelegate(ClipboardBrowser *view, const ClipboardBrowserSharedPtr &sharedData, QWidget *parent = nullptr);

        ~ItemDelegate();

        QSize sizeHint(const QModelIndex &index) const;

        QSize sizeHint(const QStyleOptionViewItem &option,
                       const QModelIndex &index) const override;

        bool eventFilter(QObject *obj, QEvent *event) override;

        /** Remove item widget if not currently visible and return true if removed. */
        bool invalidateHidden(QWidget *widget);

        /** Set regular expression for highlighting. */
        void setSearch(const QRegularExpression &re);

        /** Return regular expression for highlighting. */
        const QRegularExpression &searchExpression() const { return m_re; }

        /** Return cached item, create it if it doesn't exist. */
        ItemWidget *cache(const QModelIndex &index);

        /**
         * Update data to display.
         */
        void updateCache(QObject *widget, const QVariantMap &data);

        /** Return cached item or nullptr. */
        ItemWidget *cacheOrNull(int row) const;

        /** Set maximum size for all items. */
        void setItemSizes(QSize size, int idealWidth);

        /** Create internal item editor widget. */
        ItemEditorWidget *createCustomEditor(QWidget *parent, const QModelIndex &index,
                                             bool editNotes);

        /**
         * Highlight matched text with current serch expression, font and color.
         */
        void highlightMatches(ItemWidget *itemWidget) const;

        /**
         * Set or unset item widget as current.
         *
         * Only current item widget should receive mouse events (e.g. text selection).
         *
         * This allows interaction only for current item widget.
         */
        void setItemWidgetCurrent(const QModelIndex &index, bool isCurrent);

        /**
         * Set or unset item widget as selected.
         *
         * This changes item appearace according to current theme/style.
         */
        void setItemWidgetSelected(const QModelIndex &index, bool isSelected);

        void dataChanged(const QModelIndex &a, const QModelIndex &b);
        void rowsRemoved(const QModelIndex &parent, int start, int end);
        void rowsInserted(const QModelIndex &parent, int start, int end);
        void rowsMoved(const QModelIndex &parent, int sourceStart, int sourceEnd,
                       const QModelIndex &destination, int destinationRow);

        bool showAt(const QModelIndex &index, QPoint pos);

        QWidget *createPreview(const QVariantMap &data, QWidget *parent);

    signals:
        void itemWidgetCreated(const PersistentDisplayItem &selection);

    protected:
        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    private:
        void setIndexWidget(const QModelIndex &index, ItemWidget *w);

        void setWidgetCurrent(QWidget *ww, bool isCurrent);

        /// Updates style for selected/unselected widgets.
        void setWidgetSelected(QWidget *ww, bool selected);

        int findWidgetRow(const QObject *obj) const;

        ItemWidget *updateCache(const QModelIndex &index, const QVariantMap &data);

        ClipboardBrowser *m_view;
        ClipboardBrowserSharedPtr m_sharedData;
        QRegularExpression m_re;
        QSize m_maxSize;
        int m_idealWidth;

        std::vector<std::shared_ptr<ItemWidget>> m_cache;
};

#endif // ITEMDELEGATE_H

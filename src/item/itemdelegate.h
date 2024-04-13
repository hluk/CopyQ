// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMDELEGATE_H
#define ITEMDELEGATE_H

#include "item/itemfilter.h"
#include "gui/clipboardbrowsershared.h"

#include <QItemDelegate>
#include <QRegularExpression>
#include <QTimer>

#include <memory>
#include <vector>

class Item;
class ItemEditorWidget;
class ItemFactory;
class ItemWidget;
class ClipboardBrowser;
class PersistentDisplayItem;

constexpr int defaultItemHeight = 100;

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
        void setItemFilter(const ItemFilterPtr &filter);

        /** Return regular expression for highlighting. */
        const ItemFilterPtr &itemFilter() const { return m_filter; }

        /** Creates item widget if not created already. */
        void createItemWidget(const QModelIndex &index);

        /**
         * Update data to display.
         */
        void updateWidget(QObject *widget, const QVariantMap &data);

        /** Set maximum size for all items. */
        void setItemSizes(int maxWidth, int idealWidth);

        /** Create internal item editor widget. */
        ItemEditorWidget *createCustomEditor(
                QWidget *parent, const QModelIndex &index, const QString &format);

        /**
         * Highlight matched text with current search expression, font and color.
         */
        void highlightMatches(ItemWidget *itemWidget) const;

        /// Update positions of all widgets
        /// (after filtering out items or invalidating).
        void updateAllRows();
        void updateLater();
        void updateIfNeeded();

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

        QWidget *createPreview(const QVariantMap &data, QWidget *parent);

        void setCurrentRow(int row, bool current);

    signals:
        void itemWidgetCreated(const PersistentDisplayItem &selection);

    protected:
        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    private:
        struct Item {
            ItemWidget *operator->() const noexcept { return item.get(); }
            ItemWidget *get() const noexcept { return item.get(); }
            operator bool() const noexcept { return static_cast<bool>(item); }

            std::shared_ptr<ItemWidget> item;
            int appliedFilterId = 0;
            QSize size = QSize(0, defaultItemHeight);
        };

        void setIndexWidget(const QModelIndex &index, ItemWidget *w);

        /// Updates style for selected/unselected widgets.
        void setWidgetSelected(QWidget *ww, bool selected);

        int findWidgetRow(const QObject *obj) const;

        ItemWidget *updateWidget(const QModelIndex &index, const QVariantMap &data);

        void updateItemWidgetSize(int row);
        void updateItemSize(const QModelIndex &index, QSize itemWidgetSize);

        QPoint findPositionForWidget(const QModelIndex &index) const;

        void invalidateAllHiddenNow();

        ClipboardBrowser *m_view;
        ClipboardBrowserSharedPtr m_sharedData;
        ItemFilterPtr m_filter;
        int m_filterId = 0;
        int m_maxWidth;
        int m_idealWidth;

        QTimer m_timerInvalidateHidden;

        std::vector<Item> m_items;
};

#endif // ITEMDELEGATE_H

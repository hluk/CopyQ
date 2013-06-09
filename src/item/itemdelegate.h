/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include <QItemDelegate>
#include <QRegExp>
#include <QSharedPointer>

class Item;
class ItemWidget;

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
class ItemDelegate : public QItemDelegate
{
    Q_OBJECT

    public:
        ItemDelegate(QWidget *parent = NULL);

        ~ItemDelegate();

        QSize sizeHint(const QModelIndex &index) const;

        QSize sizeHint(const QStyleOptionViewItem &option,
                       const QModelIndex &index) const;

        QWidget *createEditor(QWidget *parent,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const;

        void setEditorData(QWidget *editor, const QModelIndex &index) const;

        void setModelData(QWidget *editor, QAbstractItemModel *model,
                          const QModelIndex &index) const;

        bool eventFilter(QObject *object, QEvent *event);

        /** Remove all cached items (cache is refreshed using paint()). */
        void invalidateCache();

        /** Set regular expression for highlighting. */
        void setSearch(const QRegExp &re);

        /** Search highlight style. */
        void setSearchStyle(const QFont &font, const QPalette &palette);

        /** Editor widget style. */
        void setEditorStyle(const QFont &font, const QPalette &palette);

        /** Item number style. */
        void setNumberStyle(const QFont &font, const QPalette &palette);

        /** Show/hide item number. */
        void setShowNumber(bool show) { m_showNumber = show; }

        /** Return cached item, create it if it doesn't exist. */
        ItemWidget *cache(const QModelIndex &index);

        /** Return true only if item at index is already in cache. */
        bool hasCache(const QModelIndex &index) const;

        /** Set maximum size for all items. */
        void setItemMaximumSize(const QSize &size);

        /** Save edited item on return or ctrl+return. */
        void setSaveOnEnterKey(bool enable) { m_saveOnReturnKey = enable; }

        /** Update row position. */
        void updateRowPosition(int row, const QPoint &position);

        /** Hide row. */
        void hideRow(int row);

        /** Show/hide row. */
        void setRowVisible(int row, bool visible);

        /** Use next item loader available for @a index. */
        void nextItemLoader(const QModelIndex &index);

        /** Use previous item loader available for @a index. */
        void previousItemLoader(const QModelIndex &index);

        /** Enable/disable editing notes. */
        void setEditNotes(bool editNotes);

    signals:
        /** User begins or stops to edit an item in a tab. */
        void editingActive(bool active);

        /** Emitted if @a row size changes. */
        void rowSizeChanged(int row);

    protected:
        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    private:
        QWidget *m_parent;
        bool m_showNumber;
        bool m_saveOnReturnKey;
        QRegExp m_re;
        QSize m_maxSize;
        bool m_editNotes;

        QFont m_foundFont;
        QPalette m_foundPalette;
        QFont m_editorFont;
        QPalette m_editorPalette;
        QFont m_numberFont;
        int m_numberWidth;
        QPalette m_numberPalette;

        QList< QSharedPointer<ItemWidget> > m_cache;

        void setIndexWidget(const QModelIndex &index, ItemWidget *w);

    public slots:
        // change size buffer
        void dataChanged(const QModelIndex &a, const QModelIndex &b);
        void rowsRemoved(const QModelIndex &parent, int start, int end);
        void rowsInserted(const QModelIndex &parent, int start, int end);
        void rowsMoved(const QModelIndex &sourceParent,
                       int sourceStart, int sourceEnd,
                       const QModelIndex &destinationParent,
                       int destinationRow);
        void editorSave();
        void editorCancel();

    private slots:
        void editingStarts();
        void editingStops();
};

#endif

/*
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
#include <QLabel>
#include <QTimer>

/**
 * Delegate for items in ClipboardBrowser.
 *
 * Creates editor on demand and draws contents of all items.
 * To achieve better performance the first call to get sizeHint value for
 * an item returns some default value (so it doesn't have to render the item
 * needlessly). Trying to paint the item will not succeed instead it notifies
 * all listeners that the size has changed (signal sizeHintChanged).
 */
class ItemDelegate : public QItemDelegate
{
    Q_OBJECT

    public:
        ItemDelegate(QWidget *parent = 0);

        QSize sizeHint (const QStyleOptionViewItem &option, const QModelIndex &index) const;

        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
        void setEditorData(QWidget *editor, const QModelIndex &index) const;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
        bool eventFilter(QObject *object, QEvent *event);

        void setStyleSheet(const QString &css);

        void invalidateCache() const;

        void setDryPaint(bool dryPaint) { m_dryPaint = dryPaint; }
        bool dryPaint() const { return m_dryPaint; }

        void setSearch(const QRegExp &re);

    protected:
        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    private:
        QWidget *m_parent;
        QString m_css;
        int m_maxsize;
        bool m_dryPaint;
        QRegExp m_re;

        // items drawn using QTextDocument
        mutable QList<QWidget*> m_cache;

        // get size and/or pixmap from cache
        QWidget *cache(const QModelIndex &index) const;
        void removeCache(int row) const;
        void removeCache(const QModelIndex &index) const;

    signals:
        void sizeUpdated(const QModelIndex &index) const;

    public slots:
        // change size buffer
        void dataChanged(const QModelIndex &a, const QModelIndex &b);
        void rowsRemoved(const QModelIndex&,int,int);
        void rowsInserted(const QModelIndex & parent, int start, int end);
        void rowsMoved(const QModelIndex & sourceParent, int sourceStart, int sourceEnd,
                       const QModelIndex & destinationParent, int destinationRow);
};

#endif

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

#include <QStyledItemDelegate>

#include <QRegExp>

class QTextDocument;

class ItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
        ItemDelegate(QWidget *parent = 0);
        ~ItemDelegate();

        QSize sizeHint (const QStyleOptionViewItem &option, const QModelIndex &index) const;

        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
        void setEditorData(QWidget *editor, const QModelIndex &index) const;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
        bool eventFilter(QObject *object, QEvent *event);

        void setStyleSheet(const QString &css);

        void setItemFormat(const QString &format) {
            m_format = format;
        }

        const QString &itemFormat() const {
            return m_format;
        }

        void invalidateCache() const;

    protected:
        void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;

    private:
        QWidget *m_parent;
        QString m_css;
        QString m_format;
        int m_maxsize;

        // items drawn using QTextDocument
        mutable QList<QTextDocument*> m_cache;
        // create QTextDocument for given item and save size to buffer
        QTextDocument *getCache(const QModelIndex &index, QSize *size = NULL) const;
        void removeCache(int row) const;
        void removeCache(const QModelIndex &index) const;

    public slots:
        // change size buffer
        void dataChanged(const QModelIndex &a, const QModelIndex &b);
        void rowsRemoved(const QModelIndex&,int,int);
        void rowsInserted(const QModelIndex & parent, int start, int end);
        void rowsMoved(const QModelIndex & sourceParent, int sourceStart, int sourceEnd,
                       const QModelIndex & destinationParent, int destinationRow);
};

#endif

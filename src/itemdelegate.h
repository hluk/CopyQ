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
#include <QTextDocument>
#include <QRegExp>
#include <QDebug>

class ItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
        ItemDelegate(QObject *parent = 0);

        QSize sizeHint (const QStyleOptionViewItem &option, const QModelIndex &index) const;
        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
        void setEditorData(QWidget *editor, const QModelIndex &index) const;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
        bool eventFilter(QObject *object, QEvent *event);

        QString getHtml(QString &body, int row = 99) const;

        void setStyleSheet(const QString &css) {
            doc->setDefaultStyleSheet(css);
        }

    protected:
        void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;

private:
        QTextDocument* doc;
};

#endif

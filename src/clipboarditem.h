/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

#ifndef CLIPBOARDITEM_H
#define CLIPBOARDITEM_H

#include <QTextDocument>
#include <QStyleOptionViewItem>
#include <QRegExp>

class ClipboardItem
{
    public:
        ClipboardItem () {};
        ClipboardItem (const QString &txt, const QString &css, const QRegExp &search);
        void setData(const QString &txt, const QRegExp &search);
        QString text() const { return m_txt; };
        QRegExp searchExp() const { return m_search; };
        QSize size() const { return m_size; };
        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    private:
        // create document and cache
        void updateCache() const;
        // plain text
        QString m_txt;
        //highlighted expression
        QRegExp m_search;
        // document template
        QString m_doctmp;
        // document cache
        mutable QString m_cache;
        // size
        mutable QSize m_size;
};

Q_DECLARE_METATYPE(ClipboardItem)

#endif

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

#ifndef CLIPBOARDMODEL_H
#define CLIPBOARDMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QRegExp>
#include <QList>
#include <QDebug>
#include <QImage>
#include <QHash>

static QString escape(const QString &str)
{
    static const QChar ch_newline('\n');
    static const QChar ch_space(' ');
    static const QChar ch_tab('\t');
    static const QChar ch_gt('>');
    static const QChar ch_lt('<');
    static const QChar ch_amp('&');
    static const QString html_space("&nbsp;");
    static const QString html_newline("<br />");
    static const QString html_gt("&gt;");
    static const QString html_lt("&lt;");
    static const QString html_amp("&amp;");
    QString res;

    for ( QString::const_iterator it = str.begin(); it < str.end(); ++it ) {
        if( *it == ch_newline )
            res += html_newline;
        else if( *it == ch_space || *it == ch_tab )
            res += html_space;
        else if( *it == ch_gt )
            res += html_gt;
        else if( *it == ch_lt )
            res += html_lt;
        else if( *it == ch_amp )
            res += html_amp;
        else
            res += *it;
    }

    return res;
}

static const QModelIndex empty_index;

class ClipboardItem : public QString
{
public:
    ClipboardItem(const QString &str) : QString(str),
        m_highlight(NULL), m_image(NULL), m_filtered(false) {}

    ~ClipboardItem()
    {
        removeHighlight();
        removeImage();
    }

    void setHighlight(const QString &str) const
    {
        if (m_highlight)
            *m_highlight = str;
        else
            m_highlight = new QString(str);
    }

    const QString &highlighted() const
    {
        if ( !m_highlight )
            setHighlight( escape(*(dynamic_cast<const QString*>(this))) );
        return *m_highlight;
    }

    void removeHighlight()
    {
        if (m_highlight) {
            delete m_highlight;
            m_highlight = NULL;
        }
    }

    void removeImage()
    {
        if ( m_image ) {
            delete m_image;
            m_image =  NULL;
        }
    }

    bool isFiltered() const { return m_filtered; }
    void setFiltered(bool filtered) {
        m_filtered = filtered;
        removeHighlight();
    }

    // image
    void setImage(const QImage &image) {
        removeHighlight();
        clear();
        append( QString("IMAGE") );
        if (!m_image)
            m_image = new QImage(image);
        else
            *m_image = image;
    }

    const QImage *image() const {
        return m_image;
    }

private:
    mutable QString *m_highlight;
    QImage *m_image;
    bool m_filtered;
};

class ClipboardModel : public QAbstractListModel
{
public:
    ClipboardModel(const QStringList &items = QStringList());

    // need to be implemented
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    
    // editting
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool setData(const QModelIndex &index, const QVariant &value,
                  int role = Qt::EditRole);
    bool insertRows(int position, int rows, const QModelIndex &index = QModelIndex());
    bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex());
    void clear() {
        m_clipboardList.clear();
    }

    void setMaxItems(int max) { m_max = max; }

    bool move(int pos, int newpos);

    // search
    const QRegExp *search() const { return &m_re; }
    void setSearch(const QRegExp *const re = NULL);
    void setSearch(int i);
    bool isFiltered(int i) const; // is item filtered out

    int getRowNumber(int row, bool cycle = false) const;

    bool isImage(int row) const {
        return row < rowCount() &&
                m_clipboardList[row].image() != NULL;
    }
    const QImage image(int row) const {
        if ( isImage(row) )
            return *(m_clipboardList[row].image());
        else
            return QImage();
    }

private:
    QList<ClipboardItem> m_clipboardList;
    QRegExp m_re;
    int m_max;
};

#endif // CLIPBOARDMODEL_H

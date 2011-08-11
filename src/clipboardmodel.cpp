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

#include "clipboardmodel.h"
#include "clipboarditem.h"
#include <QMap>

QString escape(const QString &str)
{
    /*
    QString res(str);
    res.replace('&', "&amp;")
       .replace(' ', "&nbsp;")
       .replace('\t', "&nbsp;&nbsp;")
       .replace('>', "&gt;")
       .replace('<', "&lt;")
       .replace('\n', "<br />");
       */
    QString res;
    static QRegExp re("[<>& \t\n]");
    int a = 0, b = 0, n = 0;
    while( (b = str.indexOf(re, a)) != -1 ) {
        n = b-a;
        if (n)
            res.append( str.mid(a, n) );
        switch( str.at(b).toAscii() ) {
        case '<': res.append("&lt;"); break;
        case '>': res.append("&gt;"); break;
        case '&': res.append("&amp;"); break;
        case ' ': res.append("&nbsp;"); break;
        case '\t': res.append("&nbsp;&nbsp;"); break;
        case '\n': res.append("<br />"); break;
        }
        a = b+1;
    }
    res.append( str.mid(a) );

    return res;
}

ClipboardModel::ClipboardModel(QObject *parent) :
        QAbstractListModel(parent)
{
    m_formats << QString("image/x-inkscape-svg-compressed")
              << QString("image/bmp")
              << QString("text/html")
              << QString("text/plain");
}

void ClipboardModel::setFormats(const QString &list)
{
    m_formats = list.split( QRegExp("[;, ]+") );
}

int ClipboardModel::rowCount(const QModelIndex&) const
{
    return m_clipboardList.count();
}

void ClipboardModel::setFormat(int row, const QString &mimeType)
{
    m_clipboardList[row]->setFormat(mimeType);
}

void ClipboardModel::nextFormat(int row)
{
    ClipboardItem *item = m_clipboardList[row];
    QStringList formats = item->formats();

    if (formats.isEmpty())
        return;

    int i = formats.indexOf(item->format());
    if (i==-1 || i == formats.length()-1)
        item->setFormat( formats.at(0) );
    else
        item->setFormat( formats[i+1] );

    QModelIndex ind = index(row);
    emit dataChanged(ind, ind);
}

void ClipboardModel::previousFormat(int row)
{
    ClipboardItem *item = m_clipboardList[row];
    QStringList formats = item->formats();

    if (formats.isEmpty())
        return;

    int i = formats.indexOf(item->format());
    if (i <= 0)
        item->setFormat( formats.last() );
    else
        item->setFormat( formats[i-1] );

    QModelIndex ind = index(row);
    emit dataChanged(ind, ind);
}

QMimeData *ClipboardModel::mimeData(int row) const
{
    if (row < rowCount()) {
        return m_clipboardList[row]->data();
    } else {
        return NULL;
    }
}

ClipboardItem *ClipboardModel::at(int row) const
{
    return m_clipboardList[row];
}

QVariant ClipboardModel::data(int row) const
{
    return data( index(row,0), Qt::EditRole );
}

QVariant ClipboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_clipboardList.size())
        return QVariant();

    return m_clipboardList[index.row()]->data(role);
}

Qt::ItemFlags ClipboardModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
}

bool ClipboardModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        int row = index.row();
        m_clipboardList[row]->setData(value);
        setSearch(row);
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

bool ClipboardModel::setData(const QModelIndex &index, QMimeData *value)
{
    if (index.isValid()) {
        int row = index.row();
        m_clipboardList[row]->setData(value);
        setSearch(row);
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

bool ClipboardModel::append(ClipboardItem *item)
{
    int rows = rowCount();
    beginInsertRows(empty_index, rows, rows);
    m_clipboardList.append(item);
    endInsertRows();
    return true;
}

bool ClipboardModel::insertRows(int position, int rows, const QModelIndex&)
{
    ClipboardItem *item;
    beginInsertRows(empty_index, position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        item = new ClipboardItem(this);
        m_clipboardList.insert(position, item);
    }

    endInsertRows();
    return true;
}

bool ClipboardModel::removeRows(int position, int rows, const QModelIndex&)
{
    beginRemoveRows(empty_index, position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        delete m_clipboardList.takeAt(position);
    }

    endRemoveRows();
    return true;
}

int ClipboardModel::getRowNumber(int row, bool cycle) const
{
    int n = rowCount();
    if (n == 0)
        return -1;
    if (row >= n)
        return cycle ? 0 : n-1;
    else if (row < 0)
        return cycle ? n-1 : 0;
    else
        return row;
}

void ClipboardModel::setMaxItems(int max)
{
    m_max = max>0 ? max : 0;
    int rows = m_clipboardList.length();

    if (max >= rows)
        return;

    // crop list
    beginRemoveRows(empty_index, max, rows-1 );

    while ( rows > max ) {
        delete m_clipboardList.takeLast();
        --rows;
    }

    endRemoveRows();
}

bool ClipboardModel::move(int pos, int newpos)
{
    int from = getRowNumber(pos,true);
    int to   = getRowNumber(newpos,true);

    if( from == -1 || to == -1 )
        return false;
    if ( !beginMoveRows(empty_index, from, from, empty_index,
                        from < to ? to+1 : to) )
        return false;
    m_clipboardList.move(from, to);
    endMoveRows();
    return true;
}

/**
@fn  moveItems
@arg list items to move
@arg key move items in given direction (Qt::Key_Down, Qt::Key_Up, Qt::Key_End, Qt::Key_Home)
@return true if some item was moved to the top (item to clipboard), otherwise false
*/
bool ClipboardModel::moveItems(QModelIndexList list, int key) {
    qSort(list.begin(),list.end());
    int from, to;
    bool res = false;

    for(int i = 0; i<list.length(); ++i) {
        if (key == Qt::Key_Down || key == Qt::Key_End)
            from = list.at(list.length()-1-i).row();
        else
            from = list.at(i).row();

        switch (key) {
        case Qt::Key_Down:
            to = from+1;
            break;
        case Qt::Key_Up:
            to = from-1;
            break;
        case Qt::Key_End:
            to = rowCount()-i-1;
            break;
        default:
            to = 0+i;
            break;
        }
        if ( !move(from, to) )
            return false;
        if (!res)
            res = to==0 || from==0 || to == rowCount();
    }

    return res;
}

bool ClipboardModel::isFiltered(int i) const
{
    return m_clipboardList[i]->isFiltered();
}

void ClipboardModel::setSearch(int row)
{
    ClipboardItem *item = m_clipboardList[row];
    if ( m_re.isEmpty() || m_re.indexIn(item->text()) != -1 ) {
        item->setFiltered(false);
    } else {
        item->setFiltered(true);
    }
}

void ClipboardModel::setSearch(const QRegExp *const re)
{
    if (!re) {
        if ( m_re.isEmpty() )
            return; // search already empty

        m_re = QRegExp();
    } else if ( m_re == *re ) {
        return; // search already set
    } else {
        m_re = *re;
    }

    // filter items
    for( int i = 0; i<rowCount(); i++) {
        setSearch(i);
    }

    emit dataChanged( index(0,0), index(rowCount()-1,0) );
}

QDataStream &operator<<(QDataStream &stream, const ClipboardModel &model)
{
    int length = model.rowCount();
    stream << length;

    // save in reverse order so the items
    // can be later restored in right order
    for(int i = 0; i < length; ++i)
        stream << *model.at(i);

    return stream;
}

QDataStream &operator>>(QDataStream &stream, ClipboardModel &model)
{
    int length;
    stream >> length;

    ClipboardItem *item;
    for(int i = 0; i < length; ++i) {
        item = new ClipboardItem(&model);
        stream >> *item;
        model.append(item);
    }

    return stream;
}

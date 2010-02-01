#include "clipboardmodel.h"
#include <QDebug>

ClipboardModel::ClipboardModel(const QStringList &items)
    : QAbstractListModel()
{
    foreach( QString str, items) {
        m_clipboardList.append(str);
    }

    m_re.setCaseSensitivity(Qt::CaseInsensitive);
}

int ClipboardModel::rowCount(const QModelIndex&) const
{
    return m_clipboardList.count();
}

QVariant ClipboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_clipboardList.size())
        return QVariant();

    const QImage *image = m_clipboardList[index.row()].image();
    if ( image && (role == Qt::DisplayRole || role == Qt::EditRole) )
            return *image;
    else if (role == Qt::DisplayRole)
        return m_clipboardList[index.row()].highlighted();
    else if (role == Qt::EditRole)
        return m_clipboardList.at(index.row());
    else
        return QVariant();
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
        if ( value.type() == QVariant::Image )
            m_clipboardList[row].setImage( value.value<QImage>() );
        else
            m_clipboardList.replace( row, value.toString() );
        setSearch(row);
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

bool ClipboardModel::insertRows(int position, int rows, const QModelIndex&)
{
    beginInsertRows(empty_index, position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_clipboardList.insert( position, QString() );
    }

    endInsertRows();
    return true;
}

bool ClipboardModel::removeRows(int position, int rows, const QModelIndex&)
{
    beginRemoveRows(empty_index, position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_clipboardList.removeAt(position);
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

bool ClipboardModel::move(int pos, int newpos) {
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

bool ClipboardModel::isFiltered(int i) const
{
    return m_clipboardList[i].isFiltered();
}

void ClipboardModel::setSearch(int i)
{
    const QString &str = m_clipboardList[i];
    QString highlight;

    int a(0), b, len;

    if ( m_re.isEmpty() ) {
        m_clipboardList[i].setFiltered(false);
        return;
    }

    while ((b = m_re.indexIn(str, a)) != -1) {
         len = m_re.matchedLength();
         if ( len == 0 )
             break;
         highlight += ESCAPE(str.mid(a, b-a)) +
                        "<span class=\"em\">" +
                        ESCAPE(str.mid(b, len)) +
                        "</span>";
         a = b + len;
    }
    // filter items
    if ( highlight.isEmpty() )
        m_clipboardList[i].setFiltered(true);
    else {
        if ( a != str.length() )
            highlight += ESCAPE(str.mid(a));
        // highlight matched
        m_clipboardList[i].setFiltered(false);
        m_clipboardList[i].setHighlight(highlight);
    }
}

void ClipboardModel::setSearch(const QRegExp *const re)
{
    if (!re) {
        if ( m_re.isEmpty() )
            return; // search already empty

        m_re = QRegExp();
    }
    else if ( m_re == *re )
        return; // search already set
    else
        m_re = *re;

    for( int i = 0; i<rowCount(); i++)
        setSearch(i);

    emit dataChanged( index(0,0), index(rowCount()-1,0) );
}

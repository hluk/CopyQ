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
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_clipboardList.size())
        return QVariant();

    if (role == Qt::DisplayRole)
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
        m_clipboardList.replace(index.row(), value.toString());
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

bool ClipboardModel::insertRows(int position, int rows, const QModelIndex&)
{
    beginInsertRows(QModelIndex(), position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_clipboardList.insert( position, QString() );
    }

    endInsertRows();
    return true;
}

bool ClipboardModel::removeRows(int position, int rows, const QModelIndex&)
{
    beginRemoveRows(QModelIndex(), position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_clipboardList.removeAt(position);
    }

    endRemoveRows();
    return true;
}

bool ClipboardModel::move(int pos, int newpos) {
    if( pos < 0 || newpos < 0 || newpos >= rowCount() || newpos >= rowCount() )
        return false;
    if ( !beginMoveRows(QModelIndex(), pos, pos, QModelIndex(),
                        pos < newpos ? newpos+1 : newpos) )
        return false;
    m_clipboardList.move(pos,newpos);
    endMoveRows();
    return true;
}

bool ClipboardModel::isFiltered(int i) const
{
    return m_clipboardList[i].isFiltered();
}

void ClipboardModel::setSearch(const QRegExp *const re)
{
    if (!re) {
        if ( m_re.isEmpty() )
            return; // search already empty

        m_re = QRegExp();
        for( int i = 0; i<rowCount(); i++) {
            m_clipboardList[i].removeHighlight();
            m_clipboardList[i].setFiltered(false);
        }
    }
    else {
        if ( m_re == *re )
            return; // search already set

        m_re = *re;
        for( int i = 0; i<rowCount(); i++) {
            const QString &str = m_clipboardList[i];
            QString highlight;

            int a(0), b, len;
            // test regexp
            if (m_re.indexIn(QString(), 0) != -1) {
                // regexp was st. like ".*"
                highlight = "<span class=\"em\">"
                            + ESCAPE(str) +
                            "</span>";
                a = str.length();
            }
            else {
                // highlight all
                while ((b = m_re.indexIn(str, a)) != -1) {
                     len = m_re.matchedLength();
                     highlight += ESCAPE(str.mid(a, b-a)) +
                                    "<span class=\"em\">" +
                                    ESCAPE(str.mid(b, len)) +
                                    "</span>";
                     a = b+len;
                }
            }
            if ( a != str.length() )
                highlight += ESCAPE(str.mid(a));

            m_clipboardList[i].highlight(highlight);
            m_clipboardList[i].setFiltered(a==0);
        }
    }
    emit dataChanged( index(0,0), index(rowCount()-1,0) );
}

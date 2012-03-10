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

#include <QString>
#include <QVariant>
#include <QMimeData>
#include <QStringList>

class ClipboardModel;

QString escape(const QString &str);

class ClipboardItem
{
public:
    ClipboardItem(const ClipboardModel *parent=NULL);
    ~ClipboardItem();

    QString text() const;

    // highlight matched text
    QString highlightedHtml() const;

    void clear();

    void setData(QMimeData *data);
    void setData(const QString &mimeType, const QByteArray &data);
    void setData(const QVariant &value);
    QVariant data(int role) const;
    QMimeData *data () const { return m_data; }

    void setFormat(const QString &mimeType);
    QStringList formats() const { return m_data->formats(); }
    const QString &format() const { return m_mimeType; }
    void setPreferredFormat();

    bool isFiltered() const { return m_filtered; }
    void setFiltered(bool filtered) { m_filtered = filtered; }

    void setDefault(const QString & mimeType) { m_mimeType = mimeType; }

private:
    const ClipboardModel *m_parent;
    QMimeData *m_data;
    QString m_mimeType;
    bool m_filtered;
};

QDataStream &operator<<(QDataStream &stream, const ClipboardItem &item);
QDataStream &operator>>(QDataStream &stream, ClipboardItem &item);

#endif // CLIPBOARDITEM_H

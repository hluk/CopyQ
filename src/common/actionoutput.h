/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#ifndef ACTIONOUTPUT_H
#define ACTIONOUTPUT_H

class ActionOutputItems : public QObject
{
    Q_OBJECT
public:
    explicit ActionOutputItems(Action *action);

    void setOutputFormat(const QString &outputItemFormat) { m_outputFormat = outputItemFormat; }
    void setItemSeparator(const QRegExp &itemSeparator) { m_sep = itemSeparator; }
    void setOutputTab(const QString &outputTabName) { m_tab = outputTabName; }

private:
    QRegExp m_sep;
    QString m_tab;
    QString m_outputFormat;
    QString m_lastOutput;
};

class ActionOutputItem : public QObject
{
    Q_OBJECT
public:
    explicit ActionOutputItem(Action *action);

    void setOutputFormat(const QString &outputItemFormat) { m_outputFormat = outputItemFormat; }
    void setOutputTab(const QString &outputTabName) { m_tab = outputTabName; }

private:
    QString m_tab;
    QString m_outputFormat;
    QByteArray m_lastData;
};

class ActionOutputIndex : public QObject
{
    Q_OBJECT
public:
    ActionOutputIndex(const QModelIndex &index, Action *action)
        : QObject(action)
        , m_index(index)
    {
    }

private slots:
    void onStandardOutput(const QByteArray &output);

private:
    QPersistentModelIndex m_index;
};

#endif // ACTIONOUTPUT_H

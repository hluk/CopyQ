/*
    Copyright (c) 2019, Lukas Holecek <hluk@email.cz>

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

#ifndef ACTIONTABLEMODEL_H
#define ACTIONTABLEMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>

class Action;

enum class ActionState;

class ActionTableModel : public QAbstractTableModel
{
public:
    explicit ActionTableModel(QObject *parent = nullptr);

    void actionAboutToStart(Action *action);
    void actionStarted(Action *action);
    void actionFailed(Action *action, const QString &error);
    void actionFinished(Action *action);
    void actionFinished(const QString &name);

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    struct ActionData {
        QString name;
        QDateTime started;
        qint64 finished;
        QString error;
    };

    static ActionState actionState(const ActionData &data);

    ActionData &actionData(int row) { return m_actions[row]; }
    const ActionData &actionData(int row) const { return m_actions[row]; }
    int actionCount() const { return m_actions.size(); }

    std::vector<ActionData> m_actions;
};

#endif // ACTIONTABLEMODEL_H

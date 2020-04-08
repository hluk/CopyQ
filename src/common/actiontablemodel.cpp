/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "actiontablemodel.h"

#include "common/action.h"
#include "common/actionhandlerenums.h"

#include <QColor>

#include <algorithm>

namespace {

constexpr auto dateTimeFormat = "yyyy-MM-dd HH:mm:ss.zzz";

QString actionStateToString(ActionState state)
{
    switch (state) {
    case ActionState::Error: return "Error";
    case ActionState::Finished: return "Finished";
    case ActionState::Running: return "Running";
    case ActionState::Starting: return "Starting";
    }
    return QString();
}

int actionStateOrder(ActionState state)
{
    switch (state) {
    case ActionState::Error: return 1;
    case ActionState::Finished: return 0;
    case ActionState::Running: return 2;
    case ActionState::Starting: return 3;
    }
    return -1;
}

} // namespace

ActionTableModel::ActionTableModel(uint maxRowCount, QObject *parent)
    : QAbstractTableModel(parent)
    , m_maxRowCount(maxRowCount)
{
}

int ActionTableModel::actionAboutToStart(Action *action)
{
    ActionData actionData;
    actionData.id = m_actions.empty() ? 1 : m_actions[m_actions.size() - 1].id + 1;
    actionData.name = action->name();
    if ( actionData.name.isEmpty() )
        actionData.name = action->commandLine();
    actionData.finished = -1;

    limitItems();

    beginInsertRows(QModelIndex(), actionCount(), actionCount());
    m_actions.push_back(actionData);
    endInsertRows();

    return actionData.id;
}

void ActionTableModel::actionStarted(Action *action)
{
    const int row = rowFor(action);
    actionData(row).started = QDateTime::currentDateTime();
    for (const int column : { ActionHandlerColumn::started, ActionHandlerColumn::status }) {
        const auto index = this->index(row, column);
        emit dataChanged(index, index);
    }
}

void ActionTableModel::actionFailed(Action *action, const QString &error)
{
    const int row = rowFor(action);
    actionData(row).error = error;
    for (const int column : { ActionHandlerColumn::error, ActionHandlerColumn::status }) {
        const auto index = this->index(row, column);
        emit dataChanged(index, index);
    }
}

void ActionTableModel::actionFinished(Action *action)
{
    const int row = rowFor(action);
    ActionData &data = actionData(row);
    data.finished = data.started.msecsTo(QDateTime::currentDateTime());
    for (const int column : { ActionHandlerColumn::finished, ActionHandlerColumn::status }) {
        const auto index = this->index(row, column);
        emit dataChanged(index, index);
    }
}

void ActionTableModel::actionFinished(const QString &name)
{
    ActionData actionData;
    actionData.id = m_actions.empty() ? 1 : m_actions.end()->id + 1;
    actionData.name = name;
    actionData.started = QDateTime::currentDateTime();
    actionData.finished = 0;

    limitItems();

    beginInsertRows(QModelIndex(), actionCount(), actionCount());
    m_actions.push_back(actionData);
    endInsertRows();
}

QVariant ActionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ActionHandlerColumn::id:
            return "ID";
        case ActionHandlerColumn::name:
            return "Name";
        case ActionHandlerColumn::status:
            return "Status";
        case ActionHandlerColumn::started:
            return "Started";
        case ActionHandlerColumn::finished:
            return "Finished";
        case ActionHandlerColumn::error:
            return "Error";
        }
    }

    return QVariant();
}

int ActionTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return actionCount();
}

int ActionTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return ActionHandlerColumn::count;
}

QVariant ActionTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        const int row = index.row();
        const int column = index.column();
        const ActionData &data = actionData(row);
        switch (column) {
        case ActionHandlerColumn::id:
            return row;
        case ActionHandlerColumn::name:
            return data.name;
        case ActionHandlerColumn::status:
            return actionStateToString(actionState(data));
        case ActionHandlerColumn::started:
            return data.started.toString(dateTimeFormat);
        case ActionHandlerColumn::finished:
            return data.finished == -1 ? QString() : data.started.addMSecs(data.finished).toString(dateTimeFormat);
        case ActionHandlerColumn::error:
            return data.error;
        }
    } else if (role == Qt::ToolTipRole) {
        const int row = index.row();
        const int column = index.column();
        const ActionData &data = actionData(row);
        if (column == ActionHandlerColumn::name)
            return data.name;
        if (column == ActionHandlerColumn::error)
            return data.error;
    } else if (role == Qt::DecorationRole) {
            const int column = index.column();
            if (column == ActionHandlerColumn::status) {
                const int row = index.row();
                const ActionData &data = actionData(row);
                switch (actionState(data)) {
                case ActionState::Error: return QColor(0xe56950);
                case ActionState::Running: return QColor(0xcce550);
                case ActionState::Starting: return QColor(0xe5b350);
                case ActionState::Finished: break;
                }
            }
    } else if (role >= Qt::UserRole) {
        role -= Qt::UserRole;
        if (role == ActionHandlerRole::sort) {
            const int row = index.row();
            const int column = index.column();
            const ActionData &data = actionData(row);
            switch (column) {
            case ActionHandlerColumn::id:
                return data.id;
            case ActionHandlerColumn::name:
                return data.name;
            case ActionHandlerColumn::status:
                return actionStateOrder(actionState(data));
            case ActionHandlerColumn::started:
                return data.started;
            case ActionHandlerColumn::finished:
                return data.finished;
            case ActionHandlerColumn::error:
                return data.error;
            }
        } else if (role == ActionHandlerRole::status) {
            const int row = index.row();
            const ActionData &data = actionData(row);
            return static_cast<int>(actionState(data));
        } else if (role == ActionHandlerRole::id) {
            const int row = index.row();
            const ActionData &data = actionData(row);
            return data.id;
        }
    }

    return QVariant();
}

ActionState ActionTableModel::actionState(const ActionTableModel::ActionData &data)
{
    if ( !data.error.isEmpty() )
        return ActionState::Error;
    if ( data.finished != -1 )
        return ActionState::Finished;
    if ( data.started.isValid() )
        return ActionState::Running;
    return ActionState::Starting;
}

int ActionTableModel::rowFor(const Action *action) const
{
    const auto found = std::lower_bound(
        std::begin(m_actions), std::end(m_actions), action->id(),
        [](const ActionData &data, int id) {
            return data.id < id;
        });
    const auto row = std::distance(std::begin(m_actions), found);
    return static_cast<int>(row);
}

void ActionTableModel::limitItems()
{
    if (m_actions.size() < m_maxRowCount * 4 / 3)
        return;

    while (m_actions.size() >= m_maxRowCount) {
        const auto found = std::find_if(
            std::begin(m_actions), std::end(m_actions),
            [](const ActionData &data) {
                const auto state = actionState(data);
                return state == ActionState::Finished || state == ActionState::Error;
            });

        if ( found == std::end(m_actions) )
            break;

        const auto row = static_cast<int>( std::distance(std::begin(m_actions), found) );
        beginRemoveRows(QModelIndex(), row, row);
        m_actions.erase(found);
        endRemoveRows();
    }
}

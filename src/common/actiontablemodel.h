// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACTIONTABLEMODEL_H
#define ACTIONTABLEMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>

class Action;

enum class ActionState;

class ActionTableModel final : public QAbstractTableModel
{
public:
    explicit ActionTableModel(QObject *parent = nullptr);

    void setMaxRowCount(uint rows);

    int actionAboutToStart(Action *action);
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
        int id;
        QString name;
        QDateTime started;
        qint64 finished;
        QString error;
    };

    static ActionState actionState(const ActionData &data);

    ActionData &actionData(int row) { return m_actions[row]; }
    const ActionData &actionData(int row) const { return m_actions[row]; }
    int rowFor(const Action *action) const;
    int actionCount() const { return m_actions.size(); }
    void limitItems();

    std::vector<ActionData> m_actions;
    uint m_maxRowCount = 1000;
};

#endif // ACTIONTABLEMODEL_H

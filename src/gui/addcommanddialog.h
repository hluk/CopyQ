// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ADDCOMMANDDIALOG_H
#define ADDCOMMANDDIALOG_H

#include "common/command.h"

#include <QDialog>
#include <QtContainerFwd>

namespace Ui {
class AddCommandDialog;
}

class QSortFilterProxyModel;

class AddCommandDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AddCommandDialog(const QVector<Command> &pluginCommands, QWidget *parent = nullptr);
    ~AddCommandDialog();

    void accept() override;

signals:
    void addCommands(const QVector<Command> &commands);

private:
    void onLineEditFilterTextChanged(const QString &text);
    void onListViewCommandsActivated();

    Ui::AddCommandDialog *ui;
    QSortFilterProxyModel *m_filterModel;
};

#endif // ADDCOMMANDDIALOG_H

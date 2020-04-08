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

#ifndef ADDCOMMANDDIALOG_H
#define ADDCOMMANDDIALOG_H

#include <QDialog>
#include <QVector>

namespace Ui {
class AddCommandDialog;
}

class QSortFilterProxyModel;
struct Command;

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
    void onFilterLineEditFilterChanged(const QRegularExpression &re);
    void onListViewCommandsActivated();

    Ui::AddCommandDialog *ui;
    QSortFilterProxyModel *m_filterModel;
};

#endif // ADDCOMMANDDIALOG_H

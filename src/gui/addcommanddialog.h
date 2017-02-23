/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

namespace Ui {
class AddCommandDialog;
}

class QSortFilterProxyModel;
struct Command;

class AddCommandDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddCommandDialog(const QList<Command> &pluginCommands, QWidget *parent = nullptr);
    ~AddCommandDialog();

public slots:
    void accept() override;

signals:
    void addCommands(const QList<Command> &commands);

private slots:
    void on_filterLineEdit_filterChanged(const QRegExp &re);
    void on_listViewCommands_activated();

private:
    Ui::AddCommandDialog *ui;
    QSortFilterProxyModel *m_filterModel;
};

#endif // ADDCOMMANDDIALOG_H

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

#ifndef IMPORTEXPORTDIALOG_H
#define IMPORTEXPORTDIALOG_H

#include <QDialog>

namespace Ui {
class ImportExportDialog;
}

class ImportExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportExportDialog(QWidget *parent = nullptr);
    ~ImportExportDialog();

    void setTabs(const QStringList &tabs);
    void setCurrentTab(const QString &tabName);

    void setHasConfiguration(bool hasConfiguration);
    void setHasCommands(bool hasCommands);

    void setConfigurationEnabled(bool enabled);
    void setCommandsEnabled(bool enabled);

    QStringList selectedTabs() const;
    bool isConfigurationEnabled() const;
    bool isCommandsEnabled() const;

private slots:
    void on_checkBoxAll_clicked(bool checked);

    void update();

private:
    bool canAccept() const;

    Ui::ImportExportDialog *ui;
};

#endif // IMPORTEXPORTDIALOG_H

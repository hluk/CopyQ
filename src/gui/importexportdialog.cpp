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

#include "gui/importexportdialog.h"
#include "ui_importexportdialog.h"

#include "gui/tabicons.h"

#include <QPushButton>

ImportExportDialog::ImportExportDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ImportExportDialog)
{
    ui->setupUi(this);

    connect( ui->listTabs, SIGNAL(itemSelectionChanged()),
             this, SLOT(update()) );
    connect( ui->checkBoxConfiguration, SIGNAL(stateChanged(int)),
             this, SLOT(update()) );
    connect( ui->checkBoxCommands, SIGNAL(stateChanged(int)),
             this, SLOT(update()) );
}

ImportExportDialog::~ImportExportDialog()
{
    delete ui;
}

void ImportExportDialog::setTabs(const QStringList &tabs)
{
    ui->listTabs->clear();
    ui->listTabs->addItems(tabs);
    ui->listTabs->selectAll();
    const auto items = ui->listTabs->selectedItems();
    for (const auto item : items)
        item->setIcon( getIconForTabName(item->text()) );
}

void ImportExportDialog::setCurrentTab(const QString &tabName)
{
    const auto items = ui->listTabs->findItems(tabName, Qt::MatchExactly);
    if ( !items.isEmpty() )
        ui->listTabs->setCurrentItem( items.first() );
}

QStringList ImportExportDialog::selectedTabs() const
{
    QStringList result;
    const auto items = ui->listTabs->selectedItems();
    for (const auto item : items)
        result.append( item->text() );
    return result;
}

bool ImportExportDialog::isConfigurationEnabled() const
{
    return ui->checkBoxConfiguration->isChecked();
}

bool ImportExportDialog::isCommandsEnabled() const
{
    return ui->checkBoxCommands->isChecked();
}

void ImportExportDialog::on_checkBoxAll_clicked(bool cheked)
{
    ui->checkBoxConfiguration->setChecked(cheked);
    ui->checkBoxCommands->setChecked(cheked);

    if (cheked)
        ui->listTabs->selectAll();
    else
        ui->listTabs->clearSelection();
}

void ImportExportDialog::update()
{
    const bool ok = canAccept();

    auto button = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (button)
        button->setEnabled(ok);

    if (!ok) {
        ui->checkBoxAll->setCheckState(Qt::Unchecked);
    } else if ( ui->listTabs->selectedItems().count() == ui->listTabs->count()
                && ui->checkBoxConfiguration->isChecked()
                && ui->checkBoxCommands->isChecked() )
    {
        ui->checkBoxAll->setCheckState(Qt::Checked);
    } else {
        ui->checkBoxAll->setCheckState(Qt::PartiallyChecked);
    }
}

bool ImportExportDialog::canAccept() const
{
    return ui->listTabs->selectionModel()->hasSelection()
            || ui->checkBoxConfiguration->isChecked()
            || ui->checkBoxCommands->isChecked();
}

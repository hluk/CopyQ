// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui/importexportdialog.h"
#include "ui_importexportdialog.h"

#include "gui/tabicons.h"

#include <QPushButton>

ImportExportDialog::ImportExportDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ImportExportDialog)
{
    ui->setupUi(this);

    connect(ui->checkBoxAll, &QCheckBox::clicked,
            this, &ImportExportDialog::onCheckBoxAllClicked);

    connect( ui->listTabs, &QListWidget::itemSelectionChanged,
             this, &ImportExportDialog::update );
    connect( ui->checkBoxConfiguration, &QCheckBox::stateChanged,
             this, &ImportExportDialog::update );
    connect( ui->checkBoxCommands, &QCheckBox::stateChanged,
             this, &ImportExportDialog::update );
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

    const bool showTabs = ui->listTabs->count() > 0;
    ui->listTabs->setVisible(showTabs);
    ui->labelTabs->setVisible(showTabs);
}

void ImportExportDialog::setCurrentTab(const QString &tabName)
{
    const auto items = ui->listTabs->findItems(tabName, Qt::MatchExactly);
    if ( !items.isEmpty() )
        ui->listTabs->setCurrentItem( items.first() );
}

void ImportExportDialog::setHasConfiguration(bool hasConfiguration)
{
    ui->checkBoxConfiguration->setVisible(hasConfiguration);
}

void ImportExportDialog::setHasCommands(bool hasCommands)
{
    ui->checkBoxCommands->setVisible(hasCommands);
}

void ImportExportDialog::setConfigurationEnabled(bool enabled)
{
    ui->checkBoxConfiguration->setChecked(enabled);
}

void ImportExportDialog::setCommandsEnabled(bool enabled)
{
    ui->checkBoxCommands->setChecked(enabled);
}

QStringList ImportExportDialog::selectedTabs() const
{
    const auto items = ui->listTabs->selectedItems();
    QStringList result;
    result.reserve( items.size() );
    for (const auto item : items)
        result.append( item->text() );
    return result;
}

bool ImportExportDialog::isConfigurationEnabled() const
{
    return ui->checkBoxConfiguration->isChecked()
            && !ui->checkBoxConfiguration->isHidden();
}

bool ImportExportDialog::isCommandsEnabled() const
{
    return ui->checkBoxCommands->isChecked()
            && !ui->checkBoxCommands->isHidden();
}

void ImportExportDialog::onCheckBoxAllClicked(bool checked)
{
    ui->checkBoxConfiguration->setChecked(checked);
    ui->checkBoxCommands->setChecked(checked);

    if (checked)
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
                && (ui->checkBoxConfiguration->isChecked() || ui->checkBoxConfiguration->isHidden())
                && (ui->checkBoxCommands->isChecked() || ui->checkBoxCommands->isHidden()) )
    {
        ui->checkBoxAll->setCheckState(Qt::Checked);
    } else {
        ui->checkBoxAll->setCheckState(Qt::PartiallyChecked);
    }
}

bool ImportExportDialog::canAccept() const
{
    return ui->listTabs->selectionModel()->hasSelection()
            || isConfigurationEnabled()
            || isCommandsEnabled();
}

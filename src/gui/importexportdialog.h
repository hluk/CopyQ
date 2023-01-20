// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IMPORTEXPORTDIALOG_H
#define IMPORTEXPORTDIALOG_H

#include <QDialog>

namespace Ui {
class ImportExportDialog;
}

class ImportExportDialog final : public QDialog
{
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

private:
    void onCheckBoxAllClicked(bool checked);

    void update();

    bool canAccept() const;

    Ui::ImportExportDialog *ui;
};

#endif // IMPORTEXPORTDIALOG_H

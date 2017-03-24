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

#ifndef COMMANDDIALOG_H
#define COMMANDDIALOG_H

#include "common/command.h"
#include "common/commandstore.h"

#include <QDialog>

namespace Ui {
class CommandDialog;
}

class QAbstractButton;

class CommandDialog : public QDialog
{
    Q_OBJECT

public:
    CommandDialog(
            const Commands &pluginCommands, const QStringList &formats,
            QWidget *parent = nullptr);
    ~CommandDialog();

    /** Create new commands. */
    void addCommands(const Commands &commands);

    void apply();

    bool maybeClose(QWidget *saveMessageBoxParent);

public slots:
    void reject() override;

signals:
    void commandsSaved();

private slots:
    void tryPasteCommandFromClipboard();
    void copySelectedCommandsToClipboard();
    void onCommandDropped(const QString &text, int row);

    void onCurrentCommandWidgetIconChanged(const QString &iconString);
    void onCurrentCommandWidgetNameChanged(const QString &name);
    void onCurrentCommandWidgetAutomaticChanged(bool automatic);

    void onFinished(int result);

    void on_itemOrderListCommands_addButtonClicked();
    void on_itemOrderListCommands_itemSelectionChanged();
    void on_pushButtonLoadCommands_clicked();
    void on_pushButtonSaveCommands_clicked();
    void on_pushButtonCopyCommands_clicked();
    void on_pushButtonPasteCommands_clicked();
    void on_lineEditFilterCommands_textChanged(const QString &text);
    void on_buttonBox_clicked(QAbstractButton* button);

    void onAddCommands(const QList<Command> &commands);

    void onClipboardChanged();

private:
    Commands currentCommands() const;

    void addCommandsWithoutSave(const Commands &commands, int targetRow);
    Commands selectedCommands() const;
    QString serializeSelectedCommands();
    bool hasUnsavedChanges() const;

    Ui::CommandDialog *ui;
    Commands m_savedCommands;

    Commands m_pluginCommands;
    QStringList m_formats;
};

#endif // COMMANDDIALOG_H

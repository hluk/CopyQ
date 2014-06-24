/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include <QDialog>

namespace Ui {
class CommandDialog;
}

class QAbstractButton;
struct Command;

class CommandDialog : public QDialog
{
    Q_OBJECT

public:
    typedef QList<Command> Commands;

    explicit CommandDialog(QWidget *parent = NULL);
    ~CommandDialog();

    void loadSettings();

    /** Return enabled commands. */
    Commands commands(bool onlyEnabled = true, bool onlySaved = true) const;

    /** Create new command. */
    void addCommand(const Command &command);

    void apply();

signals:
    void commandsSaved();

private slots:
    void tryPasteCommandFromClipboard();
    void copySelectedCommandsToClipboard();
    void onCommandDropped(const QString &text, int row);

    void onCurrentCommandWidgetIconChanged(const QString &iconString);
    void onCurrentCommandWidgetNameChanged(const QString &name);

    void onFinished(int result);

    void on_itemOrderListCommands_addButtonClicked(QAction *action);
    void on_itemOrderListCommands_itemSelectionChanged();
    void on_pushButtonLoadCommands_clicked();
    void on_pushButtonSaveCommands_clicked();
    void on_lineEditFilterCommands_textChanged(const QString &text);
    void on_buttonBox_clicked(QAbstractButton* button);

private:
    void addCommandWithoutSave(const Command &command, int targetRow = -1);
    QIcon getCommandIcon(const QString &iconString) const;
    void loadCommandsFromFile(const QString &fileName, bool unindentCommand, int targetRow);
    Commands selectedCommands() const;
    bool defaultCommand(int index, Command *c) const;

    Ui::CommandDialog *ui;
};

CommandDialog::Commands loadCommands(bool onlyEnabled = true);
void saveCommands(const CommandDialog::Commands &commands);

#endif // COMMANDDIALOG_H

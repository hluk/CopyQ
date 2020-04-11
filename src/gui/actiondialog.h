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

#ifndef ACTIONDIALOG_H
#define ACTIONDIALOG_H

#include <QDialog>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QRegularExpression>
#include <QVariantMap>

class QAbstractButton;
struct Command;

namespace Ui {
    class ActionDialog;
}

/** Dialog class for executing Command objects. */
class ActionDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ActionDialog(QWidget *parent = nullptr);
    ~ActionDialog();

    /** Restore command history from configuration. */
    void restoreHistory();

    /** Save command history. */
    void saveHistory();

    /** Return filename for storing command history. */
    const QString dataFilename() const;

    /** Set action input data. */
    void setInputData(const QVariantMap &data);
    /** Set command for dialog. */
    void setCommand(const Command &cmd);
    /** Set texts for tabs in combo box. */
    void setOutputTabs(const QStringList &tabs);

    void setCurrentTab(const QString &currentTabName);

    /** Load settings. */
    void loadSettings();

    /** Return current command. */
    Command command() const;

signals:
    /** Emitted if dialog was accepted. */
    void accepted(const Command &command, const QStringList &arguments, const QVariantMap &data);

    void saveCommand(const Command &command);

private:
    void onButtonBoxClicked(QAbstractButton* button);
    void onComboBoxCommandsCurrentIndexChanged(int index);
    void onComboBoxInputFormatCurrentIndexChanged(const QString &format);
    void onComboBoxOutputFormatEditTextchanged(const QString &text);
    void onComboBoxOutputTabEditTextChanged(const QString &text);
    void onSeparatorEditTextEdited(const QString &text);

    void previousCommand();
    void nextCommand();

    void acceptCommand();
    QVariant createCurrentItemData();
    void saveCurrentCommandToHistory();

    Ui::ActionDialog *ui;
    QVariantMap m_data;

    int m_currentCommandIndex;
};

#endif // ACTIONDIALOG_H

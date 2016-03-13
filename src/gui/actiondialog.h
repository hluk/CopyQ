/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include <QRegExp>
#include <QVariantMap>

class Action;
class QAbstractButton;
struct Command;

namespace Ui {
    class ActionDialog;
}

/** Dialog class for creating Action objects. */
class ActionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ActionDialog(QWidget *parent = 0);
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
    void setOutputTabs(const QStringList &tabs, const QString &currentTabName);
    /** Set output item. */
    void setOutputIndex(const QModelIndex &index);

    /** Load settings. */
    void loadSettings();

    /** Return current command. */
    Command command() const;

public slots:
    /** Create action from dialog's content. */
    void createAction();

    void accept();
    void done(int r);

signals:
    /** Emitted if dialog was accepted. */
    void accepted(
            Action *action //!< The accepted action.
            );

    void saveCommand(const Command &command);

    void closed(ActionDialog *self);

private slots:
    void on_buttonBox_clicked(QAbstractButton* button);
    void on_comboBoxCommands_currentIndexChanged(int index);
    void on_comboBoxInputFormat_currentIndexChanged(const QString &format);
    void on_comboBoxOutputFormat_editTextChanged(const QString &text);
    void on_comboBoxOutputTab_editTextChanged(const QString &text);
    void on_separatorEdit_textEdited(const QString &text);

private:
    QVariant createCurrentItemData();
    void saveItemData();

    Ui::ActionDialog *ui;
    QVariantMap m_data;
    QPersistentModelIndex m_index;
    QStringList m_capturedTexts;

    int m_currentCommandIndex;

    QString m_actionName;
};

#endif // ACTIONDIALOG_H

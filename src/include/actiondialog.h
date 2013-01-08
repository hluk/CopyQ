/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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
#include <QRegExp>

class Action;
class QAbstractButton;

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

    /** Set action input text. */
    void setInputText(const QString &input);
    /** Set command with arguments. */
    void setCommand(const QString &cmd);
    /** Item separator for command output. */
    void setSeparator(const QString &sep);
    /** If set to true input text will be sent to command's standard input. */
    void setInput(bool value);
    /** If set to true command's standard output will be split to items. */
    void setOutput(bool value);
    /** Set texts for tabs in combo box. */
    void setOutputTabs(const QStringList &tabs, const QString &currentTabName);
    /** Set regular expression. */
    void setRegExp(const QRegExp &re);

    /** Load settings. */
    void loadSettings();

    /** Save settings. */
    void saveSettings();

protected:
    void showEvent(QShowEvent *e);
    void accept();

private:
    Ui::ActionDialog *ui;
    QRegExp m_re;

signals:
    /** Emitted if dialog was accepted. */
    void accepted(
            Action *action //!< The accepted action.
            );

private slots:
    void on_buttonBox_clicked(QAbstractButton* button);
    void on_outputCheckBox_toggled(bool checked);
    void updateMinimalGeometry();

public slots:
    /** Create action from dialog's content. */
    void createAction();
};

#endif // ACTIONDIALOG_H

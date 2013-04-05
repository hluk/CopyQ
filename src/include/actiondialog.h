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
#include <QModelIndex>
#include <QRegExp>

class Action;
class QAbstractButton;
class QMimeData;

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
    void setInputData(const QMimeData &data);
    /** Set command with arguments. */
    void setCommand(const QString &cmd);
    /** Item separator for command output. */
    void setSeparator(const QString &sep);
    /** Send data of given MIME type to stdin of program. */
    void setInput(const QString &format);
    /** Create items from stdout of program. */
    void setOutput(const QString &format);
    /** Set texts for tabs in combo box. */
    void setOutputTabs(const QStringList &tabs, const QString &currentTabName);
    /** Set regular expression. */
    void setRegExp(const QRegExp &re);
    /** Set output item. */
    void setOutputIndex(const QModelIndex &index);

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
    QMimeData *m_data;
    QModelIndex m_index;

signals:
    /** Emitted if dialog was accepted. */
    void accepted(
            Action *action //!< The accepted action.
            );

private slots:
    void on_buttonBox_clicked(QAbstractButton* button);
    void on_comboBoxInputFormat_currentIndexChanged(const QString &format);
    void on_comboBoxOutputFormat_editTextChanged(const QString &text);
    void updateMinimalGeometry();

public slots:
    /** Create action from dialog's content. */
    void createAction();
};

#endif // ACTIONDIALOG_H

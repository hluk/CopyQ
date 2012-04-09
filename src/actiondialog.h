/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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

class QCompleter;
class Action;
class QAbstractButton;

namespace Ui {
    class ActionDialog;
}

class ActionDialog : public QDialog {
    Q_OBJECT
public:
    ActionDialog(QWidget *parent = 0);
    ~ActionDialog();
    void setInputText(const QString &input);
    void restoreHistory();
    void saveHistory();
    void add(const QString &command);
    const QString dataFilename() const;

    void setCommand(const QString &cmd);
    void setSeparator(const QString &sep);
    void setInput(bool value);
    void setOutput(bool value);
    void setOutputTabs(const QStringList &tabs, const QString &currentTabName);

protected:
    void changeEvent(QEvent *e);
    void showEvent(QShowEvent *e);

private:
    Ui::ActionDialog *ui;
    int m_maxitems;
    QCompleter *m_completer;
    QStringList m_history;
    int m_actions;

signals:
    void addItems(const QStringList &lst, const QString &tabName);
    void error(const QString &msg);
    void message(const QString &title, const QString &msg);
    void actionFinished(Action *act);
    void addMenuItem(QAction *menuItem);
    void removeMenuItem(QAction *menuItem);
    void changeTrayIcon(const QIcon &icon);

private slots:
    void on_buttonBox_clicked(QAbstractButton* button);
    void on_outputCheckBox_toggled(bool checked);
    void closeAction(Action *act);
    void onFinnished(int);

public slots:
    void runCommand();
    void accept() {}
};

#endif // ACTIONDIALOG_H

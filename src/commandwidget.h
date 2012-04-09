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

#ifndef COMMANDWIDGET_H
#define COMMANDWIDGET_H

#include <QWidget>
#include <QIcon>

struct Command {
    Command()
        : name()
        , re()
        , cmd()
        , sep()
        , input(false)
        , output(false)
        , wait(false)
        , automatic(false)
        , ignore(false)
        , enable(true)
        , icon()
        , shortcut()
        , tab()
        , outputTab()
        {}

    QString name;
    QRegExp re;
    QString cmd;
    QString sep;
    bool input;
    bool output;
    bool wait;
    bool automatic;
    bool ignore;
    bool enable;
    QString icon;
    QString shortcut;
    QString tab;
    QString outputTab;
};

namespace Ui {
class CommandWidget;
}

class QComboBox;

class CommandWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommandWidget(QWidget *parent = 0);
    ~CommandWidget();

    Command command() const;
    void setCommand(const Command &command) const;

    void setTabs(const QStringList &tabs);

private slots:
    void on_pushButtonBrowse_clicked();

    void on_lineEditIcon_textChanged(const QString &arg1);

    void on_pushButtonShortcut_clicked();

    void on_lineEditCommand_textChanged(const QString &arg1);

    void on_checkBoxOutput_toggled(bool checked);

private:
    Ui::CommandWidget *ui;

    void setTabs(const QStringList &tabs, QComboBox *w);
};

#endif // COMMANDWIDGET_H

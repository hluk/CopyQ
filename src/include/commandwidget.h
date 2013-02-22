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

#ifndef COMMANDWIDGET_H
#define COMMANDWIDGET_H

#include <QWidget>

namespace Ui {
class CommandWidget;
}

class QComboBox;
struct Command;

/** Widget (set of widgets) for creating or modifying Command object. */
class CommandWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommandWidget(QWidget *parent = NULL);
    ~CommandWidget();

    /** Return command for the widget. */
    Command command() const;

    /** Set current command. */
    void setCommand(const Command &command);

    /** Set list of known tab names (for combo box). */
    void setTabs(const QStringList &tabs);

    /** Set possible formats (for combo box). */
    void setFormats(const QStringList &formats);

private slots:
    void on_pushButtonBrowse_clicked();

    void on_lineEditIcon_textChanged(const QString &arg1);

    void on_pushButtonShortcut_clicked();

    void on_lineEditCommand_textChanged(const QString &arg1);

    void on_comboBoxOutputFormat_editTextChanged(const QString &format);

    void onIconChanged(QAction *action);

private:
    Ui::CommandWidget *ui;

    void setTabs(const QStringList &tabs, QComboBox *w);
};

#endif // COMMANDWIDGET_H

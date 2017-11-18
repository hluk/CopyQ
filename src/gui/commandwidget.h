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
    explicit CommandWidget(QWidget *parent = nullptr);
    ~CommandWidget();

    /** Return command for the widget. */
    Command command() const;

    /** Set current command. */
    void setCommand(const Command &c);

    /** Set formats for format selection combo boxes. */
    void setFormats(const QStringList &formats);

signals:
    void iconChanged();

    void nameChanged(const QString &name);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void on_lineEditName_textChanged(const QString &text);

    void on_buttonIcon_currentIconChanged();

    void on_checkBoxShowAdvanced_stateChanged(int state);

    void on_checkBoxAutomatic_stateChanged(int);

    void on_checkBoxInMenu_stateChanged(int);

    void on_checkBoxIsScript_stateChanged(int);

    void on_checkBoxGlobalShortcut_stateChanged(int);

    void on_shortcutButtonGlobalShortcut_shortcutAdded(const QKeySequence &shortcut);

    void on_shortcutButtonGlobalShortcut_shortcutRemoved(const QKeySequence &shortcut);

    void on_commandEdit_changed();

private:
    void init();

    void updateWidgets();

    void emitIconChanged();

    Ui::CommandWidget *ui;
};

#endif // COMMANDWIDGET_H

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

#ifndef COMMANDWIDGET_H
#define COMMANDWIDGET_H

#include <QWidget>

namespace Ui {
class CommandWidget;
}

class QComboBox;
struct Command;

/** Widget (set of widgets) for creating or modifying Command object. */
class CommandWidget final : public QWidget
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

    void commandTextChanged(const QString &command);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void onLineEditNameTextChanged(const QString &text);

    void onButtonIconCurrentIconChanged();

    void onCheckBoxShowAdvancedStateChanged(int state);

    void onCommandEditCommandTextChanged(const QString &command);

    void init();

    void updateWidgets();

    void emitIconChanged();

    QString description() const;

    Ui::CommandWidget *ui;
};

#endif // COMMANDWIDGET_H

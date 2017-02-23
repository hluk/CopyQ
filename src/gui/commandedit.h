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

#ifndef COMMANDEDIT_H
#define COMMANDEDIT_H

#include <QWidget>

namespace Ui {
class CommandEdit;
}

class CommandEdit : public QWidget
{
    Q_OBJECT

public:
    explicit CommandEdit(QWidget *parent = nullptr);
    ~CommandEdit();

    void setCommand(const QString &command) const;
    QString command() const;

    bool isEmpty() const;

    QFont commandFont() const;

signals:
    void changed();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void on_plainTextEditCommand_textChanged();

private:
    void updateCommandEditSize();

    Ui::CommandEdit *ui;
};

#endif // COMMANDEDIT_H

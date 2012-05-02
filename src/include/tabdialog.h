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

#ifndef TAGDIALOG_H
#define TAGDIALOG_H

#include <QDialog>
#include <QStringList>

namespace Ui {
    class TabDialog;
}

class TabDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TabDialog(QWidget *parent = 0);
    ~TabDialog();

    /** Set existing tabs for validation. */
    void setTabs(const QStringList &tabs);

    /** Set current tab name. */
    void setTabName(const QString &tabName);

private:
    Ui::TabDialog *ui;
    QStringList m_tabs;

signals:
    void accepted(const QString &name);

private slots:
    void onAccepted();
    void validate();
};

#endif // TAGDIALOG_H

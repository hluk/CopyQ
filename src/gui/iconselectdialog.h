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

#ifndef ICONSELECTDIALOG_H
#define ICONSELECTDIALOG_H

#include <QDialog>

class QModelIndex;

class IconListWidget;

class IconSelectDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit IconSelectDialog(const QString &defaultIcon, QWidget *parent = nullptr);

    const QString &selectedIcon() const { return m_selectedIcon; }

    void done(int result) override;

signals:
    void iconSelected(const QString &icon);

private:
    void onIconListItemActivated(const QModelIndex &index);

    void onBrowse();

    void onAcceptCurrent();

    void addIcons();

    IconListWidget *m_iconList;
    QString m_selectedIcon;
};

#endif // ICONSELECTDIALOG_H

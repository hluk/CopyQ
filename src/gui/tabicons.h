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
#ifndef TABICONS_H
#define TABICONS_H

class QIcon;
class QComboBox;
class QString;
class QStringList;
class QWidget;

/** Return list of saved tabs (ordered by "tabs" option if possible). */
QStringList savedTabs();

QString getIconNameForTabName(const QString &tabName);

void setIconNameForTabName(const QString &name, const QString &icon);

QIcon getIconForTabName(const QString &tabName);

void initTabComboBox(QComboBox *comboBox);

void setDefaultTabItemCounterStyle(QWidget *widget);

void setComboBoxItems(QComboBox *comboBox, const QStringList &items);

#endif // TABICONS_H

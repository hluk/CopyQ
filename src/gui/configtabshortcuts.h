/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#ifndef CONFIGTABSHORTCUTS_H
#define CONFIGTABSHORTCUTS_H

#include <QWidget>

namespace Ui {
class ConfigTabShortcuts;
}

class QSettings;

class ConfigTabShortcuts : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigTabShortcuts(QWidget *parent = nullptr);

    ~ConfigTabShortcuts();

    /** Load shortcuts from settings file. */
    void loadShortcuts(const QSettings &settings);
    /** Save shortcuts to settings file. */
    void saveShortcuts(QSettings *settings) const;

private:
    Ui::ConfigTabShortcuts *ui;
};

#endif // CONFIGTABSHORTCUTS_H

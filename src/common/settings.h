/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>

/**
 * Wrapper for safer QSettings() handling.
 * When destroyed, backs up application settings ("-bak" is appended to QCoreApplication::applicationName()).
 * Static method restoreSettings() (called from constructor), tries to restore settings if QSettings() is empty.
 */
class Settings : public QSettings
{
public:
    Settings();

    ~Settings();

    static bool isEmpty(const QSettings &settings);

    static void restoreSettings();

private:
    void backUp();
};


#endif // SETTINGS_H

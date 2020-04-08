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

#ifndef TEMPORARYSETTINGS_H
#define TEMPORARYSETTINGS_H

#include <QSettings>

/**
 * Temporary ini settings which is removed after destroyed.
 *
 * Use this to get ini as data instead of saving to a file.
 */
class TemporarySettings final
{
public:
    /// Creates temporary settings file.
    explicit TemporarySettings(const QByteArray &content = QByteArray());

    /// Destroys undelying settings and removes settings file.
    ~TemporarySettings();

    /// Returns underlying settings.
    QSettings *settings();

    /// Return content of settings file.
    QByteArray content();

    TemporarySettings(const TemporarySettings &) = delete;
    TemporarySettings &operator=(const TemporarySettings &) = delete;

private:
    QSettings *m_settings;
};


#endif // TEMPORARYSETTINGS_H

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

#ifndef FOREGROUNDBACKGROUNDFILTER_H
#define FOREGROUNDBACKGROUNDFILTER_H

#include <QObject>

/**
 * This event filter manages the "activationPolicy" for an OS X app by
 * ensuring that it is a "regular" app when there are windows shown, but
 * an "accessory" or "prohibited"/"background" app when there are none.
 *
 * This allows the app to not have a dock icon unless there is an open window.
 *
 * If only menu or some notifications are visible dock icon is hidden.
 */
class ForegroundBackgroundFilter final : public QObject
{
public:
    /**
     * Install the filter to parent.
     */
    static void installFilter(QObject *parent);

protected:
    bool eventFilter(QObject *obj, QEvent *ev);
    ForegroundBackgroundFilter(QObject *parent);
};

#endif // FOREGROUNDBACKGROUNDFILTER_H

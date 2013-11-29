/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "macactivity.h"

#include <common/common.h>

#include <QString>

#include <Cocoa/Cocoa.h>

MacActivity::MacActivity(ActivityType type, const QString &reason) :
        m_activity(0)
{
    NSActivityOptions options = NSActivityBackground;;
    switch (type) {
        case User:
            options = NSActivityUserInitiated;
            break;
        case Background:
            options = NSActivityBackground;
            break;
    }

    id act = [[NSProcessInfo processInfo]
        beginActivityWithOptions:options
        reason:reason.toNSString()];
    if (act) {
        m_activity = reinterpret_cast<void*>(act);
        ::log(QString("Started %1 activity for: %2").arg(type == User ? "User": "Background").arg(reason), LogNote);
    } else {
        ::log("Failed to create activity", LogWarning);
    }
}

MacActivity::~MacActivity() {
    id act = reinterpret_cast<id>(m_activity);
    if (act) {
        [[NSProcessInfo processInfo] endActivity:act];
        ::log("Ended activity", LogNote);
    } else {
        ::log("Failed to stop activity", LogWarning);
    }
}

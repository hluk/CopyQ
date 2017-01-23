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

#ifndef PLATFORMSYSTEMMUTEX_H
#define PLATFORMSYSTEMMUTEX_H

#include <QSharedPointer>

class QString;

/// System-wide mutex
class PlatformSystemMutex {
public:
    virtual ~PlatformSystemMutex() {}

    /// Try lock mutex (non-blocking).
    virtual bool tryLock() = 0;

    /// Lock mutex (blocking).
    virtual bool lock() = 0;

    /// Unlock mutex.
    virtual bool unlock() = 0;

    virtual QString error() const = 0;
};

typedef QSharedPointer<PlatformSystemMutex> PlatformSystemMutexPtr;

PlatformSystemMutexPtr getPlatformSystemMutex(const QString &name);

#endif // PLATFORMSYSTEMMUTEX_H

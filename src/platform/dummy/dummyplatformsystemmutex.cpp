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

#include "platform/platformsystemmutex.h"

#include <QSharedMemory>
#include <QString>
#include <QSystemSemaphore>

namespace {

bool tryAttach(QSharedMemory *shmem)
{
    if (shmem->attach()) {
        shmem->detach();
        return true;
    }

    return false;
}

class DummyPlatformSystemMutex : public PlatformSystemMutex {
public:
    explicit DummyPlatformSystemMutex(const QString &name)
        : m_shmem(name)
    {
    }

    bool tryLock()
    {
        if (m_shmem.isAttached())
            return true;

        QSystemSemaphore createSharedMemoryGuard("shmem_create_" + m_shmem.key(), 1);
        if (createSharedMemoryGuard.acquire()) {
            /* Dummy attach and dettach operations can invoke shared memory
             * destruction if the last process attached to shared memory has
             * crashed and memory was not destroyed.
             */
            if (!tryAttach(&m_shmem) || !tryAttach(&m_shmem)) {
                if (!m_shmem.create(1)) {
                    m_error = "Failed to create shared memory: "
                            + m_shmem.errorString();
                }
            }
            createSharedMemoryGuard.release();
        } else {
            m_error = "Failed to acquire shared memory lock: "
                    + createSharedMemoryGuard.errorString();
        }

        return m_shmem.isAttached();
    }

    bool lock()
    {
        while (!tryLock()) {}
    }

    bool unlock()
    {
        return m_shmem.detach();
    }

    QString error() const
    {
        return m_error;
    }

private:
    QSharedMemory m_shmem;
    QString m_error;
};

} // namespace

PlatformSystemMutexPtr getPlatformSystemMutex(const QString &name)
{
    return PlatformSystemMutexPtr(new DummyPlatformSystemMutex(name));
}


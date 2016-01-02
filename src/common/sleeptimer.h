/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#ifndef SLEEPTIMER_H
#define SLEEPTIMER_H

#include <QCoreApplication>
#include <QElapsedTimer>

class SleepTimer
{
public:
    explicit SleepTimer(int timeoutMs)
        : m_timeoutMs(timeoutMs)
    {
        m_timer.start();
    }

    bool sleep()
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        return m_timer.elapsed() < m_timeoutMs;
    }

private:
    QElapsedTimer m_timer;
    int m_timeoutMs;
};

inline void waitFor(int ms)
{
    SleepTimer t(ms);
    while (t.sleep()) {}
}

#endif // SLEEPTIMER_H


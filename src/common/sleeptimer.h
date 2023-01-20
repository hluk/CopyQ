// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SLEEPTIMER_H
#define SLEEPTIMER_H

#include <QCoreApplication>
#include <QElapsedTimer>

class SleepTimer final
{
public:
    explicit SleepTimer(int timeoutMs, int minSleepCount = 2)
        : m_timeoutMs(timeoutMs)
        , m_minSleepCount(minSleepCount)
    {
        m_timer.start();
    }

    bool sleep()
    {
        if (m_minSleepCount <= 0 && m_timer.elapsed() >= m_timeoutMs)
            return false;

        --m_minSleepCount;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        return true;
    }

private:
    QElapsedTimer m_timer;
    int m_timeoutMs;
    int m_minSleepCount = 2;
};

inline void waitFor(int ms)
{
    SleepTimer t(ms);
    while (t.sleep()) {}
}

#endif // SLEEPTIMER_H


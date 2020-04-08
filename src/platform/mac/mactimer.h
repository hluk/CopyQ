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

#ifndef MACTIMER_H
#define MACTIMER_H

#include <QObject>

#ifdef __OBJC__
@class NSTimer;
#else
using NSTimer = void;
#endif

/**
 * Class similar to a QTimer but allows setting a tolerance, which
 * makes timers more battery-friendly on OSX.
 */
class MacTimer final : public QObject
{
    Q_OBJECT

public:
    explicit MacTimer(QObject *parent = 0);
    virtual ~MacTimer();

    void setInterval(int msec);
    int interval() const { return m_interval; }
    inline void setSingleShot(bool singleShot);

    /**
     * Set the tolerance for the timer. See NSTimer::setTolerance.
     *
     * Tolerance is ignored on OS X < 10.9.
     */
    void setTolerance(int msec);
    int tolerance() const { return m_tolerance; }

public Q_SLOTS:
    void start();
    void stop();
    inline bool isSingleShot() const { return m_singleShot; }

Q_SIGNALS:
    void timeout();

private:
    Q_DISABLE_COPY(MacTimer)

    void restart();

    int m_interval;
    int m_tolerance;
    bool m_singleShot;

    NSTimer *m_nsTimer;
};

#endif // MACTIMER_H

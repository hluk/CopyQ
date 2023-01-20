// SPDX-License-Identifier: GPL-3.0-or-later

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

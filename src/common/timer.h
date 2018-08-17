#ifndef TIMER_H
#define TIMER_H

#include <QTimer>

template <typename Receiver, typename Slot>
void initSingleShotTimer(QTimer *timer, int milliseconds, const Receiver *receiver, Slot slot)
{
    timer->setSingleShot(true);
    timer->setInterval(milliseconds);

    Q_ASSERT(receiver);
    if (receiver)
        QObject::connect( timer, &QTimer::timeout, receiver, slot, Qt::UniqueConnection );
}

#endif // TIMER_H

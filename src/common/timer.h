#ifndef TIMER_H
#define TIMER_H

#include <QTimer>

template <typename Receiver, typename Slot>
void initSingleShotTimer(QTimer *timer, int milliseconds, const Receiver *receiver, Slot slot)
{
    timer->setSingleShot(true);
    timer->setInterval(milliseconds);

    Q_ASSERT(receiver);
    if (!receiver)
        return;

    // Unique connection only works with member functions.
    const auto flags = std::is_invocable<Slot&>() ? Qt::AutoConnection : Qt::UniqueConnection;
    QObject::connect(timer, &QTimer::timeout, receiver, slot, flags);
}

#endif // TIMER_H

#ifndef PROCESSSIGNALS_H
#define PROCESSSIGNALS_H

#include <QObject>
#include <QProcess>

template <typename Receiver>
void connectProcessFinished(QProcess *process, Receiver *receiver, void (Receiver::*slot)())
{
    const auto processFinishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
    QObject::connect(
        process, processFinishedSignal, receiver,
        [receiver, slot](int, QProcess::ExitStatus) {
            (receiver->*slot)();
        });
}

template <typename Receiver>
void connectProcessFinished(QProcess *process, Receiver *receiver, void (Receiver::*slot)(int, QProcess::ExitStatus))
{
    const auto processFinishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
    QObject::connect(process, processFinishedSignal, receiver, slot);
}

template <typename Receiver, typename Slot>
void connectProcessError(QProcess *process, Receiver receiver, Slot slot)
{
    QObject::connect( process, &QProcess::errorOccurred, receiver, slot );
}

#endif // PROCESSSIGNALS_H

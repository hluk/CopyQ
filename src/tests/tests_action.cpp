// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests_action.h"

#include "common/action.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTest>
#include <QTimer>

#include <memory>

namespace {

void startWaitingAction(Action *action)
{
    // A bounded child process which does not start a CopyQ server or access
    // the clipboard. The tests explicitly stop it after exercising the wait.
    action->setCommand(QStringList{
        QCoreApplication::applicationFilePath(),
        QStringLiteral("--action-wait-helper"),
    });
    action->start();
}

} // namespace

ActionTests::ActionTests(const TestInterfacePtr &, QObject *parent)
    : QObject(parent)
{
    setProperty("CopyQ_test_id", QStringLiteral("action"));
}

void ActionTests::reentrantWaitDoesNotProcessEvents() const
{
    Action action;
    startWaitingAction(&action);
    QVERIFY(action.isRunning());

    QObject callbackContext;
    bool entered = false;
    bool nestedResult = true;
    bool processedEvent = false;
    bool nestedWaitProcessedEvents = false;
    QTimer::singleShot(0, &callbackContext, [&action, &callbackContext, &entered,
            &nestedResult, &processedEvent, &nestedWaitProcessedEvents] {
        entered = true;
        QTimer::singleShot(0, &callbackContext, [&processedEvent] { processedEvent = true; });
        nestedResult = action.waitForFinished(100);
        nestedWaitProcessedEvents = processedEvent;
        action.requestKill();
    });

    const bool finished = action.waitForFinished(5000);
    QVERIFY(entered);
    QVERIFY(!nestedResult);
    QVERIFY(!nestedWaitProcessedEvents);
    QVERIFY(finished);
}

void ActionTests::timeoutAllowsAnotherWait() const
{
    Action action;
    startWaitingAction(&action);
    QVERIFY(action.isRunning());
    QVERIFY(!action.waitForFinished(0));

    QTimer::singleShot(0, &action, &Action::requestKill);
    QVERIFY(action.waitForFinished(5000));
    QVERIFY(action.waitForFinished(0));
}

void ActionTests::differentActionsCanWait() const
{
    Action first;
    Action second;
    startWaitingAction(&first);
    startWaitingAction(&second);
    QVERIFY(first.isRunning());
    QVERIFY(second.isRunning());

    QObject callbackContext;
    bool nestedResult = false;
    QTimer::singleShot(0, &callbackContext, [&first, &second, &nestedResult] {
        QTimer::singleShot(0, &second, &Action::requestKill);
        nestedResult = second.waitForFinished(5000);
        first.requestKill();
    });

    QVERIFY(first.waitForFinished(5000));
    QVERIFY(nestedResult);
}

void ActionTests::deletedWhileWaiting() const
{
    QObject callbackContext;
    auto action = std::make_unique<Action>();
    const QPointer<Action> observer = action.get();
    startWaitingAction(action.get());
    QVERIFY(action->isRunning());

    QTimer::singleShot(0, &callbackContext, [&action] { action.reset(); });
    const bool finished = action->waitForFinished(100);
    QVERIFY(observer.isNull());
    QVERIFY(finished);
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests_action.h"

#include "common/action.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTest>
#include <QTimer>

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

void ActionTests::reentrantWaitDoesNotProcessEvents()
{
    Action action;
    startWaitingAction(&action);
    QVERIFY(action.isRunning());

    QObject callbackContext;
    bool entered = false;
    bool nestedResult = true;
    bool processedEvent = false;
    bool nestedWaitProcessedEvents = false;
    QTimer::singleShot(0, &callbackContext, [&] {
        entered = true;
        QTimer::singleShot(0, &callbackContext, [&] { processedEvent = true; });
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

void ActionTests::timeoutAllowsAnotherWait()
{
    Action action;
    startWaitingAction(&action);
    QVERIFY(action.isRunning());
    QVERIFY(!action.waitForFinished(0));

    QTimer::singleShot(0, &action, &Action::requestKill);
    QVERIFY(action.waitForFinished(5000));
    QVERIFY(action.waitForFinished(0));
}

void ActionTests::differentActionsCanWait()
{
    Action first;
    Action second;
    startWaitingAction(&first);
    startWaitingAction(&second);
    QVERIFY(first.isRunning());
    QVERIFY(second.isRunning());

    QObject callbackContext;
    bool nestedResult = false;
    QTimer::singleShot(0, &callbackContext, [&] {
        QTimer::singleShot(0, &second, &Action::requestKill);
        nestedResult = second.waitForFinished(5000);
        first.requestKill();
    });

    QVERIFY(first.waitForFinished(5000));
    QVERIFY(nestedResult);
}

void ActionTests::deletedWhileWaiting()
{
    QObject callbackContext;
    QPointer<Action> action = new Action(&callbackContext);
    startWaitingAction(action);
    QVERIFY(action->isRunning());

    QTimer::singleShot(0, &callbackContext, [&] { delete action.data(); });
    const bool finished = action->waitForFinished(100);
    QVERIFY(action.isNull());
    QVERIFY(finished);
}

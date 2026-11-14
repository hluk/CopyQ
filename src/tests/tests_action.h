// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "testinterface.h"

#include <QObject>

class ActionTests final : public QObject {
    Q_OBJECT
public:
    explicit ActionTests(const TestInterfacePtr &test = {}, QObject *parent = nullptr);

private slots:
    void reentrantWaitDoesNotProcessEvents();
    void timeoutAllowsAnotherWait();
    void differentActionsCanWait();
    void deletedWhileWaiting();
};

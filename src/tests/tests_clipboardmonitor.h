// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "testinterface.h"

#include <QObject>

class ClipboardMonitorTests final : public QObject {
    Q_OBJECT
public:
    explicit ClipboardMonitorTests(const TestInterfacePtr &test = {}, QObject *parent = nullptr);

private slots:
    void incompleteReadRetriesBeforePublishing();
    void newerChangeCancelsOlderRetry();
};

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "testinterface.h"

#include <QObject>

class PlatformTests final : public QObject {
    Q_OBJECT
public:
    explicit PlatformTests(const TestInterfacePtr &test = {}, QObject *parent = nullptr);
private slots:
    void pasteTargetWindowResolution();
};

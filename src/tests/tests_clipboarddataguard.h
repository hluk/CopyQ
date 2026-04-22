// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

class ClipboardDataGuardTests final : public QObject {
    Q_OBJECT
private slots:
    void badAllocData();
    void badAllocUrls();
    void badAllocText();
    void badAllocImageData();
    void badAllocGetUtf8Data();
    void normalDataStillWorks();
};

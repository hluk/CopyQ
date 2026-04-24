// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "testinterface.h"

#include <QObject>

class ClipboardDataGuardTests final : public QObject {
    Q_OBJECT
public:
    explicit ClipboardDataGuardTests(const TestInterfacePtr &test = {}, QObject *parent = nullptr);
private slots:
    void badAllocData();
    void badAllocUrls();
    void badAllocText();
    void badAllocImageData();
    void badAllocGetUtf8Data();
    void normalDataStillWorks();
    void overflowTreatedAsNoLimit();
    void emptyRulesIgnored();
    void negativeValueMeansNoLimit();
    void invalidRulesFallBackToDefault();
    void suffixParsing();
    void firstMatchWins();
    void emptySizeMeansNoLimit();
};

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "tests/testinterface.h"

#include <QObject>
#include <QRegularExpression>
#include <QStringList>

class QByteArray;

// Similar to QTemporaryFile but allows removing from other process.
class TemporaryFile {
public:
    TemporaryFile();
    ~TemporaryFile();
    QString fileName() const { return m_fileName; }

private:
    QString m_fileName;
};

int count(const QStringList &items, const QString &pattern);

QStringList splitLines(const QByteArray &nativeText);

bool testStderr(
    const QByteArray &stderrData,
    const QRegularExpression &ignoreRe = {});

/// Generate unique data.
QByteArray generateData();

QString appWindowTitle(const QString &text);

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QChar>
#include <QString>

inline QString fromIconId(int id) {
    return QString(QChar(id));
}

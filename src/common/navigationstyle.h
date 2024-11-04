// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#   include <QMetaType>
#endif

enum class NavigationStyle {
    Default,
    Vi,
    Emacs
};

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
Q_DECLARE_METATYPE(NavigationStyle)
#endif

// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QtGlobal>

namespace Utils {

class HostOsInfo
{
public:
    static constexpr bool isWindowsHost()
    {
#ifdef Q_OS_WIN
        return true;
#else
        return false;
#endif
    }

    static constexpr bool isMacHost()
    {
#ifdef Q_OS_MAC
        return true;
#else
        return false;
#endif
    }

    static constexpr bool isAnyUnixHost()
    {
#ifdef Q_OS_UNIX
        return true;
#else
        return false;
#endif
    }

    static constexpr Qt::KeyboardModifier controlModifier()
    {
#ifdef Q_OS_MAC
        return Qt::MetaModifier;
#else
        return Qt::ControlModifier;
#endif
    }
};

} // namespace Utils

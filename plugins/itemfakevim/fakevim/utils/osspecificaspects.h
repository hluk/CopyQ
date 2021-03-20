/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include <QString>

#include <algorithm>

#define QTC_WIN_EXE_SUFFIX ".exe"

namespace Utils {

// Add more as needed.
enum OsType { OsTypeWindows, OsTypeLinux, OsTypeMac, OsTypeOtherUnix, OsTypeOther };

namespace OsSpecificAspects {

inline QString withExecutableSuffix(OsType osType, const QString &executable)
{
    QString finalName = executable;
    if (osType == OsTypeWindows)
        finalName += QLatin1String(QTC_WIN_EXE_SUFFIX);
    return finalName;
}

inline Qt::CaseSensitivity fileNameCaseSensitivity(OsType osType)
{
    return osType == OsTypeWindows || osType == OsTypeMac ? Qt::CaseInsensitive : Qt::CaseSensitive;
}

inline Qt::CaseSensitivity envVarCaseSensitivity(OsType osType)
{
    return fileNameCaseSensitivity(osType);
}

inline QChar pathListSeparator(OsType osType)
{
    return QLatin1Char(osType == OsTypeWindows ? ';' : ':');
}

inline Qt::KeyboardModifier controlModifier(OsType osType)
{
    return osType == OsTypeMac ? Qt::MetaModifier : Qt::ControlModifier;
}

inline QString pathWithNativeSeparators(OsType osType, const QString &pathName)
{
    if (osType == OsTypeWindows) {
        const int pos = pathName.indexOf('/');
        if (pos >= 0) {
            QString n = pathName;
            std::replace(std::begin(n) + pos, std::end(n), '/', '\\');
            return n;
        }
    }
    return pathName;
}

} // namespace OsSpecificAspects
} // namespace Utils

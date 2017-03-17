/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "display.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QPoint>
#include <QStyle>

int smallIconSize()
{
    return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
}

QPoint toScreen(const QPoint &pos, int w, int h)
{
    const QRect availableGeometry = QApplication::desktop()->availableGeometry(pos);
    return QPoint(
                qMax(0, qMin(pos.x(), availableGeometry.right() - w)),
                qMax(0, qMin(pos.y(), availableGeometry.bottom() - h))
                );
}

int pointsToPixels(int points)
{
    return points * QApplication::desktop()->physicalDpiX() / 72;
}

/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "gui/screen.h"

#include <QApplication>
#include <QPoint>
#include <QScreen>
#include <QStyle>
#include <QWidget>
#include <QWindow>

namespace {

QScreen *screenForWidget(QWidget *w)
{
    if (w) {
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
        if ( w->screen() )
            return w->screen();
#endif

        const int i = screenNumberAt(w->pos());
        const auto screens = QGuiApplication::screens();
        if (0 <= i && i < screens.size())
            return screens[i];
    }

    return QGuiApplication::primaryScreen();
}

} // namespace

int smallIconSize()
{
    return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
}

QPoint toScreen(QPoint pos, QWidget *w)
{
    QWindow *window = w->windowHandle();
    if (window)
        window->setPosition(pos);
    else
        w->move(pos);

    const QRect availableGeometry = screenAvailableGeometry(*w);
    if ( !availableGeometry.isValid() )
        return pos;

    const QSize size = window ? window->size() : w->size();
    return QPoint(
                qMax(availableGeometry.left(), qMin(pos.x(), availableGeometry.right() - size.width())),
                qMax(availableGeometry.top(), qMin(pos.y(), availableGeometry.bottom() - size.height()))
                );
}

int pointsToPixels(int points, QWidget *w)
{
    QScreen *screen = screenForWidget(w);
    return static_cast<int>(points * screen->physicalDotsPerInchX() / 72.0);
}

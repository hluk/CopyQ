/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#ifndef WINDOWGEOMETRYGUARD_H
#define WINDOWGEOMETRYGUARD_H

#include <QObject>
#include <QWidget>
#include <QTimer>

class WindowGeometryGuard : public QObject
{
    Q_OBJECT

public:
    static void create(QWidget *window);
    static void blockUntilHidden(QWidget *window);

    bool eventFilter(QObject *object, QEvent *event);

private:
    explicit WindowGeometryGuard(QWidget *window);

    bool isWindowGeometryLocked() const;

    bool lockWindowGeometry();

private slots:
    void saveWindowGeometry();
    void restoreWindowGeometry();
    void unlockWindowGeometry();

private:
    QWidget *m_window;

    QTimer m_timerSaveGeometry;
    QTimer m_timerUnlockGeometry;
};

#endif // WINDOWGEOMETRYGUARD_H

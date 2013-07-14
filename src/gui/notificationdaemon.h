/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#ifndef NOTIFICATIONDAEMON_H
#define NOTIFICATIONDAEMON_H

#include <QMap>
#include <QObject>

class Notification;
class QPixmap;
class QPoint;
class QWidget;

class NotificationDaemon : public QObject
{
    Q_OBJECT
public:
    enum Position {
        Top = 0x2,
        Right = 0x4,
        Bottom = 0x8,
        Left = 0x10,
        TopRight = Top | Right,
        BottomRight = Bottom | Right,
        BottomLeft = Bottom | Left,
        TopLeft = Top | Left
    };

    explicit NotificationDaemon(QObject *parent = NULL);

    Notification *create(const QString &title, const QString &msg, const QPixmap &icon,
                         int msec, QWidget *parent, int id = -1);

    void setPosition(Position position);

private slots:
    void notificationDestroyed(QObject *notification);

private:
    /** Find ideal position for new notification. */
    QPoint findPosition(int ignoreId, Notification *notification);

    int m_lastId;
    Position m_position;
    QMap<int, Notification*> m_notifications;
};

#endif // NOTIFICATIONDAEMON_H

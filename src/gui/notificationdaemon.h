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

#ifndef NOTIFICATIONDAEMON_H
#define NOTIFICATIONDAEMON_H

#include <QColor>
#include <QList>
#include <QObject>
#include <QVariantMap>
#include <QTimer>

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
        Bottom = 0x4,
        Right = 0x8,
        Left = 0x10,
        TopRight = Top | Right,
        BottomRight = Bottom | Right,
        BottomLeft = Bottom | Left,
        TopLeft = Top | Left
    };

    explicit NotificationDaemon(QObject *parent = NULL);

    /** Create new notification or update one with same @a id (if non-negative). */
    void create(
            const QString &title, const QString &msg, ushort icon,
            int msec, bool clickToShow, int id = -1);

    /** Create new notification or update one with same @a id (if non-negative). */
    void create(
            const QVariantMap &data, int maxLines, ushort icon,
            int msec, bool clickToShow, int id = -1);

    /** Update interval to show notification with given @a id. */
    void updateInterval(int id, int msec);

    void setPosition(Position position);

    void setOffset(int horizontalPoints, int verticalPoints);

    void setMaximumSize(int maximumWidthPoints, int maximumHeightPoints);

    void updateNotifications();

    void setNotificationOpacity(qreal opacity);

    void setNotificationStyleSheet(const QString &styleSheet);

    void removeNotification(int id);

private slots:
    void onNotificationClose(Notification *notification);
    void doUpdateNotifications();

private:
    Notification *findNotification(int id);

    Notification *createNotification(int id = -1);

    int offsetX() const;
    int offsetY() const;

    int m_lastId;
    Position m_position;
    QList<Notification*> m_notifications;
    qreal m_opacity;
    int m_horizontalOffsetPoints;
    int m_verticalOffsetPoints;
    int m_maximumWidthPoints;
    int m_maximumHeightPoints;
    QString m_styleSheet;
    QTimer m_timerUpdate;
};

#endif // NOTIFICATIONDAEMON_H

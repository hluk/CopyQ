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

#ifndef NOTIFICATIONDAEMON_H
#define NOTIFICATIONDAEMON_H

#include "gui/notificationbutton.h"

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

    explicit NotificationDaemon(QObject *parent = nullptr);

    /** Create new notification or update one with same @a id (if non-empty). */
    void create(const QString &title,
            const QString &msg,
            const QString &icon,
            int msec,
            const QString &id,
            const NotificationButtons &buttons);

    /** Create new notification or update one with same @a id (if non-empty). */
    void create(
            const QVariantMap &data,
            int maxLines,
            const QString &icon,
            int msec,
            const QString &id,
            const NotificationButtons &buttons);

    /** Update interval to show notification with given @a id. */
    void updateInterval(const QString &id, int msec);

    void setPosition(Position position);

    void setOffset(int horizontalPoints, int verticalPoints);

    void setMaximumSize(int maximumWidthPoints, int maximumHeightPoints);

    void updateNotifications();

    void setNotificationOpacity(qreal opacity);

    void setNotificationStyleSheet(const QString &styleSheet);

    void removeNotification(const QString &id);

signals:
    void notificationButtonClicked(const NotificationButton &button);

private slots:
    void onNotificationClose(Notification *notification);
    void doUpdateNotifications();

private:
    Notification *findNotification(const QString &id);

    Notification *createNotification(
            const QString &id, const QString &title, const NotificationButtons &buttons);

    int offsetX() const;
    int offsetY() const;

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

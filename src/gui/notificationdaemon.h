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

class NotificationDaemon final : public QObject
{
    Q_OBJECT
public:
    explicit NotificationDaemon(QObject *parent = nullptr);

    ~NotificationDaemon();

    Notification *createNotification(const QString &id = QString());
    Notification *findNotification(const QString &id);

    void setIconColor(const QColor &color);

    void removeNotification(const QString &id);

signals:
    void notificationButtonClicked(const NotificationButton &button);

private:
    void onNotificationClose(Notification *notification);

    struct NotificationData {
        QString id;
        Notification *notification;
    };

    Notification *findNotification(Notification *notification);

    QList<NotificationData> m_notifications;
    QColor m_iconColor;
};

#endif // NOTIFICATIONDAEMON_H

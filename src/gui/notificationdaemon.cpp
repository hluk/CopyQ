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

#include "gui/notificationdaemon.h"

#include "common/common.h"
#include "gui/notification.h"
#include "gui/screen.h"

#include <QApplication>
#include <QPixmap>

NotificationDaemon::NotificationDaemon(QObject *parent)
    : QObject(parent)
{
    Notification::initConfiguration();
}

NotificationDaemon::~NotificationDaemon() = default;

void NotificationDaemon::setIconColor(const QColor &color)
{
    m_iconColor = color;
}

void NotificationDaemon::removeNotification(const QString &id)
{
    Notification *notification = findNotification(id);
    if (notification)
        onNotificationClose(notification);
}

void NotificationDaemon::onNotificationClose(Notification *notification)
{
    for (auto it = std::begin(m_notifications); it != std::end(m_notifications); ++it) {
        if (it->notification == notification) {
            m_notifications.erase(it);
            break;
        }
    }

    notification->deleteLater();
}

Notification *NotificationDaemon::findNotification(const QString &id)
{
    for (auto &notificationData : m_notifications) {
        if (notificationData.id == id)
            return notificationData.notification;
    }

    return nullptr;
}

Notification *NotificationDaemon::createNotification(const QString &id)
{
    Notification *notification = nullptr;
    if ( !id.isEmpty() )
        notification = findNotification(id);

    if (notification == nullptr) {
        notification = new Notification(m_iconColor, this);

        connect(this, &QObject::destroyed, notification, &QObject::deleteLater);
        connect( notification, &Notification::closeNotification,
                 this, &NotificationDaemon::onNotificationClose );
        connect( notification, &Notification::buttonClicked,
                 this, &NotificationDaemon::notificationButtonClicked );

        m_notifications.append(NotificationData{id, notification});
    }

    return notification;
}

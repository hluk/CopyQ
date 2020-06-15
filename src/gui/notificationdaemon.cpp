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
#include "common/display.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "common/timer.h"
#include "gui/notification.h"
#include "gui/screen.h"

#include <QApplication>
#include <QPixmap>
#include <QPoint>
#include <QVariant>

namespace {

const int notificationMarginPoints = 10;

int notificationMargin()
{
    return pointsToPixels(notificationMarginPoints);
}

bool isNotificationUnderCursor(const Notification *notification, QPoint cursor) {
    const QRect rect = notification->window()->geometry();
    return rect.contains(cursor);
}

} // namespace

NotificationDaemon::NotificationDaemon(QObject *parent)
    : QObject(parent)
    , m_position(BottomRight)
    , m_notifications()
    , m_opacity(1.0)
    , m_horizontalOffsetPoints(0)
    , m_verticalOffsetPoints(0)
    , m_maximumWidthPoints(300)
    , m_maximumHeightPoints(100)
{
    initSingleShotTimer( &m_timerUpdate, 100, this, &NotificationDaemon::doUpdateNotifications );
}

void NotificationDaemon::setPosition(NotificationDaemon::Position position)
{
    m_position = position;
}

void NotificationDaemon::setOffset(int horizontalPoints, int verticalPoints)
{
    m_horizontalOffsetPoints = horizontalPoints;
    m_verticalOffsetPoints = verticalPoints;
}

void NotificationDaemon::setMaximumSize(int maximumWidthPoints, int maximumHeightPoints)
{
    m_maximumWidthPoints = maximumWidthPoints;
    m_maximumHeightPoints = maximumHeightPoints;
}

void NotificationDaemon::updateNotifications()
{
    if ( !m_timerUpdate.isActive() )
        m_timerUpdate.start();
}

void NotificationDaemon::setNotificationOpacity(qreal opacity)
{
    m_opacity = opacity;
}

void NotificationDaemon::setNotificationStyleSheet(const QString &styleSheet)
{
    m_styleSheet = styleSheet;
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
    updateNotifications();
}

void NotificationDaemon::doUpdateNotifications()
{
    const QPoint cursor = QCursor::pos();

    // Postpone update if mouse cursor is over a notification.
    for (auto &notificationData : m_notifications) {
        auto notification = notificationData.notification;
        if ( notification->isVisible() && isNotificationUnderCursor(notification, cursor) ) {
            m_timerUpdate.start();
            return;
        }
    }

    const QRect screen = screenGeometry(0);

    int y = (m_position & Top) ? offsetY() : screen.bottom() - offsetY();

    for (auto &notificationData : m_notifications) {
        auto notification = notificationData.notification;
        notification->setOpacity(m_opacity);
        notification->setStyleSheet(m_styleSheet);
        notification->updateIcon();
        notification->setMaximumSize( pointsToPixels(m_maximumWidthPoints), pointsToPixels(m_maximumHeightPoints) );
        notification->adjust();

        do {
            int x;
            if (m_position & Left)
                x = offsetX();
            else if (m_position & Right)
                x = screen.right() - notification->width() - offsetX();
            else
                x = screen.right() / 2 - notification->width() / 2;

            if (m_position & Bottom)
                y -= notification->height();

            notification->move(x, y);

            if (m_position & Top)
                y += notification->height() + notificationMargin();
            else
                y -= notificationMargin();

            // Avoid positioning a notification under mouse cursor.
        } while( isNotificationUnderCursor(notification, cursor) );

        notification->show();
    }
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
        notification = new Notification();
        connect(this, &QObject::destroyed, notification, &QObject::deleteLater);
        connect( notification, &Notification::closeNotification,
                 this, &NotificationDaemon::onNotificationClose );
        connect( notification, &Notification::buttonClicked,
                 this, &NotificationDaemon::notificationButtonClicked );

        m_notifications.append(NotificationData{id, notification});
    }

    updateNotifications();

    return notification;
}

int NotificationDaemon::offsetX() const
{
    return pointsToPixels(m_horizontalOffsetPoints);
}

int NotificationDaemon::offsetY() const
{
    return pointsToPixels(m_verticalOffsetPoints);
}

// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui/notificationdaemon.h"

#include "common/common.h"
#include "common/display.h"
#include "common/timer.h"
#include "gui/notification.h"
#include "gui/notificationbasic.h"
#include "gui/screen.h"

#ifdef WITH_NATIVE_NOTIFICATIONS
#   include "gui/notificationnative/notificationnative.h"
#   include <QSysInfo>
#endif

#include <QApplication>
#include <QPixmap>
#include <QPoint>
#include <QVariant>
#include <QWidget>

namespace {

const int notificationMarginPoints = 10;

int notificationMargin()
{
    return pointsToPixels(notificationMarginPoints);
}

#ifdef WITH_NATIVE_NOTIFICATIONS
bool hasNativeNotifications()
{
#ifdef Q_OS_WIN
    static const bool supportsNotifications =
        !QSysInfo::productVersion().startsWith(QLatin1String("7"));
    return supportsNotifications;
#else
    return true;
#endif
}
#endif

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
    initSingleShotTimer( &m_timerUpdate, 100, this, &NotificationDaemon::doUpdateNotificationWidgets );
}

NotificationDaemon::~NotificationDaemon() = default;

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

void NotificationDaemon::updateNotificationWidgets()
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

    if (notification->widget() != nullptr)
        updateNotificationWidgets();

    notification->deleteLater();
}

void NotificationDaemon::doUpdateNotificationWidgets()
{
    const QPoint cursor = QCursor::pos();

    // Postpone update if mouse cursor is over a notification.
    for (auto &notificationData : m_notifications) {
        auto notification = notificationData.notification;
        QWidget *w = notification->widget();
        if ( w != nullptr && w->isVisible() && w->geometry().contains(cursor) ) {
            m_timerUpdate.start();
            return;
        }
    }

    const QRect screen = screenGeometry(0);

    int y = (m_position & Top) ? offsetY() : screen.bottom() - offsetY();

    for (auto &notificationData : m_notifications) {
        auto notification = notificationData.notification;
        QWidget *w = notification->widget();
        if (w == nullptr)
            continue;

        notification->setOpacity(m_opacity);
        w->setStyleSheet(m_styleSheet);
        w->setMaximumSize( pointsToPixels(m_maximumWidthPoints), pointsToPixels(m_maximumHeightPoints) );
        notification->adjust();

        // Avoid positioning a notification under mouse cursor.
        QRect rect = w->geometry();
        do {
            int x;
            if (m_position & Left)
                x = offsetX();
            else if (m_position & Right)
                x = screen.right() - rect.width() - offsetX();
            else
                x = screen.right() / 2 - rect.width() / 2;

            if (m_position & Bottom)
                y -= rect.height();

            if (m_position & Top)
                y += rect.height() + notificationMargin();
            else
                y -= notificationMargin();

            rect.moveTo(x, y);
        } while( rect.contains(cursor) );

        w->move(rect.topLeft());
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
#ifdef WITH_NATIVE_NOTIFICATIONS
        if (m_nativeNotificationsEnabled && hasNativeNotifications())
            notification = createNotificationNative(m_iconColor, this);
        else
            notification = createNotificationBasic(this);
#else
        notification = createNotificationBasic(this);
#endif

        connect(this, &QObject::destroyed, notification, &QObject::deleteLater);
        connect( notification, &Notification::closeNotification,
                 this, &NotificationDaemon::onNotificationClose );
        connect( notification, &Notification::buttonClicked,
                 this, &NotificationDaemon::notificationButtonClicked );

        m_notifications.append(NotificationData{id, notification});
    }

    if (notification->widget() != nullptr)
        updateNotificationWidgets();
    else
        QTimer::singleShot(0, notification, &Notification::show);

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

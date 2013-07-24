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

#include "gui/notificationdaemon.h"

#include "gui/notification.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QPixmap>
#include <QPoint>
#include <QVariant>

static const int notificationMargin = 8;

NotificationDaemon::NotificationDaemon(QObject *parent)
    : QObject(parent)
    , m_lastId(0)
    , m_position(BottomRight)
    , m_notifications()
    , m_colorBg(Qt::black)
    , m_colorFg(Qt::gray)
{
}

Notification *NotificationDaemon::create(const QString &title, const QString &msg,
                                         const QPixmap &icon, int msec, QWidget *parent, int id)
{
    Notification *notification = NULL;
    if (id >= 0)
        notification = m_notifications.value(id, NULL);

    const int newId = (id >= 0) ? id : -(++m_lastId);
    if (notification == NULL) {
        notification = new Notification(parent);
        setAppearance(notification);
        notification->setProperty("CopyQ_id", newId);
        m_notifications[newId] = notification;
    }

    notification->resize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    notification->setTitle(title);
    notification->setIcon(icon);
    notification->setMessage(msg);

    notification->adjust();

    notification->setMaximumSize( maximumSize() );

    QPoint pos = findPosition(newId, notification);

    connect( notification, SIGNAL(destroyed(QObject*)),
             this, SLOT(notificationDestroyed(QObject*)) );

    notification->popup(pos, msec);

    return notification;
}

void NotificationDaemon::updateInterval(int id, int msec)
{
    Notification *notification = m_notifications.value(id, NULL);
    if (notification)
        notification->setInterval(msec);
}

void NotificationDaemon::setPosition(NotificationDaemon::Position position)
{
    m_position = position;
}

QSize NotificationDaemon::maximumSize() const
{
    QRect screen = QApplication::desktop()->availableGeometry();
    int w = screen.width();
    int h = screen.height();
    w = qMax(qMin(w, 200), w / 3);
    h = qMax(qMin(h, 200), h / 3);

    return QSize(w, h);
}

void NotificationDaemon::updateAppearance()
{
    if ( m_notifications.isEmpty() )
        return;

    foreach (int id, m_notifications.keys())
        setAppearance(m_notifications[id]);
}

void NotificationDaemon::notificationDestroyed(QObject *notification)
{
    int id = notification->property("CopyQ_id").toInt();
    m_notifications.remove(id);
}

QPoint NotificationDaemon::findPosition(int ignoreId, Notification *notification)
{
    QRect screen = QApplication::desktop()->availableGeometry();

    int y = (m_position & Top) ? 0 : screen.bottom();
    foreach (int id, m_notifications.keys()) {
        if (id != ignoreId) {
            Notification *notification = m_notifications[id];
            if (m_position & Top)
                y = qMax( y, notification->y() + notification->height() );
            else
                y = qMin( y, notification->y() );
        }
    }

    if (m_position & Bottom)
        y -= notification->height() + notificationMargin;
    else
        y += notificationMargin;

    int x;
    if (m_position & Left)
        x = notificationMargin;
    else if (m_position & Right)
        x = screen.width() - notification->width() - notificationMargin;
    else
        x = screen.width() / 2 - notification->width() / 2;

    return QPoint(x, y);
}

void NotificationDaemon::setAppearance(Notification *notification)
{
    QColor bg = m_colorBg;
    const qreal opacity = bg.alphaF();
    bg.setAlpha(255);

    QPalette p = notification->palette();
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, m_colorFg);

    notification->setPalette(p);
    notification->setOpacity(opacity);
    notification->setFont(m_font);
}

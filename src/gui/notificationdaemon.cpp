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

#include "gui/notificationdaemon.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "gui/notification.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QPixmap>
#include <QPoint>
#include <QVariant>

namespace {

const int notificationMarginPoints = 10;

int notificationMargin()
{
    return pointsToPixels(notificationMarginPoints);
}

} // namespace

NotificationDaemon::NotificationDaemon(QObject *parent)
    : QObject(parent)
    , m_lastId(0)
    , m_position(BottomRight)
    , m_notifications()
    , m_opacity(1.0)
    , m_horizontalOffsetPoints(0)
    , m_verticalOffsetPoints(0)
    , m_maximumWidthPoints(300)
    , m_maximumHeightPoints(100)
{
    initSingleShotTimer( &m_timerUpdate, 100, this, SLOT(doUpdateNotifications()) );
}

void NotificationDaemon::create(
        const QString &title, const QString &msg, ushort icon, int msec, bool clickToShow, int id)
{
    Notification *notification = createNotification(id);

    notification->setTitle(title);
    notification->setIcon(icon);
    notification->setMessage(msg);
    notification->setInterval(msec);
    notification->setClickToShowEnabled(clickToShow);

    updateNotifications();
}

void NotificationDaemon::create(
        const QVariantMap &data, int maxLines, ushort icon, int msec, bool clickToShow, int id)
{
    Notification *notification = createNotification(id);

    notification->setIcon(icon);

    const int width = pointsToPixels(m_maximumWidthPoints) - 16 - 8;

    const QStringList formats = data.keys();
    const int imageIndex = formats.indexOf(QRegExp("^image/.*"));
    const QFont &font = notification->font();
    const bool isHidden = data.contains(mimeHidden);

    if ( !isHidden && data.contains(mimeText) ) {
        QString text = getTextData(data);
        const int n = text.count('\n') + 1;

        QString format;
        if (n > 1) {
            format = QObject::tr("%1<div align=\"right\"><small>&mdash; %n lines &mdash;</small></div>",
                                 "Notification label for multi-line text in clipboard", n);
        } else {
            format = QObject::tr("%1", "Notification label for single-line text in clipboard");
        }

        text = elideText(text, font, QString(), false, width, maxLines);
        text = escapeHtml(text);
        text.replace( QString("\n"), QString("<br />") );
        notification->setMessage( format.arg(text), Qt::RichText );
    } else if (!isHidden && imageIndex != -1) {
        QPixmap pix;
        const QString &imageFormat = formats[imageIndex];
        pix.loadFromData( data[imageFormat].toByteArray(), imageFormat.toLatin1() );

        const int height = maxLines * QFontMetrics(font).lineSpacing();
        if (pix.width() > width || pix.height() > height)
            pix = pix.scaled(QSize(width, height), Qt::KeepAspectRatio);

        notification->setPixmap(pix);
    } else {
        const QString text = textLabelForData(data, font, QString(), false, width, maxLines);
        notification->setMessage(text);
    }

    notification->setInterval(msec);
    notification->setClickToShowEnabled(clickToShow);

    updateNotifications();
}

void NotificationDaemon::updateInterval(int id, int msec)
{
    Notification *notification = findNotification(id);
    if (notification)
        notification->setInterval(msec);
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

void NotificationDaemon::removeNotification(int id)
{
    Notification *notification = findNotification(id);
    if (notification)
        onNotificationClose(notification);
}

void NotificationDaemon::onNotificationClose(Notification *notification)
{
    m_notifications.removeOne(notification);
    notification->deleteLater();
    updateNotifications();
}

void NotificationDaemon::doUpdateNotifications()
{
    const QRect screen = QApplication::desktop()->screenGeometry();

    int y = (m_position & Top) ? offsetY() : screen.bottom() - offsetY();

    foreach (Notification *notification, m_notifications) {
        notification->setOpacity(m_opacity);
        notification->setStyleSheet(m_styleSheet);
        notification->updateIcon();
        notification->adjust();
        notification->setMaximumSize( pointsToPixels(m_maximumWidthPoints), pointsToPixels(m_maximumHeightPoints) );

        int x;
        if (m_position & Left)
            x = offsetX();
        else if (m_position & Right)
            x = screen.width() - notification->width() - offsetX();
        else
            x = screen.width() / 2 - notification->width() / 2;

        if (m_position & Bottom)
            y -= notification->height();

        notification->move(x, y);

        if (m_position & Top)
            y += notification->height() + notificationMargin();
        else
            y -= notificationMargin();

        notification->show();
    }
}

Notification *NotificationDaemon::findNotification(int id)
{
    foreach (Notification *notification, m_notifications) {
        if (notification->id() == id)
            return notification;
    }

    return NULL;
}

Notification *NotificationDaemon::createNotification(int id)
{
    Notification *notification = NULL;
    if (id >= 0)
        notification = findNotification(id);

    const int newId = (id >= 0) ? id : -(++m_lastId);
    if (notification == NULL) {
        notification = new Notification(newId);
        connect(this, SIGNAL(destroyed()), notification, SLOT(deleteLater()));
        connect( notification, SIGNAL(closeNotification(Notification*)),
                 this, SLOT(onNotificationClose(Notification*)) );
        m_notifications.append(notification);
    }

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

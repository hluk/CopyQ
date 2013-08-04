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

#include "common/client_server.h"
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
    Notification *notification = createNotification(parent, id);

    notification->setTitle(title);
    notification->setIcon(icon);
    notification->setMessage(msg);

    popupNotification(notification, msec);

    return notification;
}

Notification *NotificationDaemon::create(const QMimeData &data, int maxLines, const QPixmap &icon,
                                         int msec, QWidget *parent, int id)
{
    Notification *notification = createNotification(parent, id);

    notification->setIcon(icon);

    const int width = maximumSize().width() - icon.width() - 16 - 8;

    QStringList formats = data.formats();
    const int imageIndex = formats.indexOf(QRegExp("^image/.*"));

    if ( data.hasText() ) {
        QString text = data.text();
        const int n = text.count('\n') + 1;

        QString format;
        if (n > 1) {
            format = QObject::tr("%1<div align=\"right\"><small>&mdash; %n lines &mdash;</small></div>",
                                 "Notification label for multi-line text in clipboard", n);
        } else {
            format = QObject::tr("%1", "Notification label for single-line text in clipboard");
        }

        text = elideText(text, m_font, QString(), false, width, maxLines);
        text = escapeHtml(text);
        text.replace( QString("\n"), QString("<br />") );
        notification->setMessage( format.arg(text), Qt::RichText );
    } else if (imageIndex != -1) {
        QPixmap pix;
        const QString &imageFormat = formats[imageIndex];
        pix.loadFromData( data.data(imageFormat), imageFormat.toLatin1() );

        const int height = maxLines * QFontMetrics(m_font).lineSpacing();
        if (pix.width() > width || pix.height() > height)
            pix = pix.scaled(QSize(width, height), Qt::KeepAspectRatio);

        notification->setPixmap(pix);
    } else {
        const QString text = textLabelForData(data, m_font, QString(), false, width, maxLines);
        notification->setMessage(text);
    }

    popupNotification(notification, msec);

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

void NotificationDaemon::onNotificationClose(Notification *notification)
{
    const int id = notification->id();
    m_notifications.remove(id);

    hideNotification(notification);

    delete notification;
}

QPoint NotificationDaemon::findPosition(Notification *notification)
{
    QRect screen = QApplication::desktop()->availableGeometry();

    int y = (m_position & Top) ? 0 : screen.bottom();
    foreach (Notification *notification2, m_notifications.values()) {
        if (notification != notification2) {
            if (m_position & Top)
                y = qMax( y, notification2->y() + notification2->height() );
            else
                y = qMin( y, notification2->y() );
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

Notification *NotificationDaemon::createNotification(QWidget *parent, int id)
{
    Notification *notification = NULL;
    if (id >= 0)
        notification = m_notifications.value(id, NULL);

    const int newId = (id >= 0) ? id : -(++m_lastId);
    if (notification == NULL) {
        notification = new Notification(newId, parent);
        setAppearance(notification);
        m_notifications[newId] = notification;
    } else {
        hideNotification(notification);
    }

    connect( notification, SIGNAL(closeNotification(Notification*)),
             this, SLOT(onNotificationClose(Notification*)) );

    return notification;
}

void NotificationDaemon::popupNotification(Notification *notification, int msec)
{
    notification->resize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    notification->adjust();

    notification->setMaximumSize( maximumSize() );

    const QPoint pos = findPosition(notification);

    notification->popup(pos, msec);
}

void NotificationDaemon::hideNotification(Notification *notification)
{
    notification->hide();

    const int y = notification->y();
    const int d = (notification->height() + notificationMargin) * ((m_position & Bottom) ? 1 : -1);

    foreach (Notification *notification2, m_notifications.values()) {
        const int y2 = notification2->y();
        if ( m_position & Bottom ? y2 < y : y2 > y )
            notification2->move( notification2->x(), y2 + d );
    }
}

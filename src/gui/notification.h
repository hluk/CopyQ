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

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include "gui/notificationbutton.h"

#include <QColor>
#include <QTimer>
#include <QObject>
#include <QPixmap>
#include <QPointer>

#include <memory>

class KNotification;
class QWidget;

class Notification final : public QObject
{
    Q_OBJECT

public:
    static void initConfiguration();

    explicit Notification(const QColor &iconColor, QObject *parent = nullptr);

    ~Notification();

    void setTitle(const QString &title);
    void setMessage(const QString &msg, Qt::TextFormat format = Qt::AutoText);
    void setPixmap(const QPixmap &pixmap);
    void setIcon(const QString &icon);
    void setIcon(ushort icon);
    void setIconColor(const QColor &color);
    void setInterval(int msec);
    void setButtons(const NotificationButtons &buttons);

    void show();

    void close();

signals:
    /** Emitted if notification needs to be closed. */
    void closeNotification(Notification *self);

    void buttonClicked(const NotificationButton &button);

private:
    void onButtonClicked(unsigned int id);
    void onDestroyed();
    void onClosed();
    void onIgnored();
    void onActivated();
    void update();

    void notificationLog(const char *message);

    KNotification *dropNotification();

    QPointer<KNotification> m_notification;
    NotificationButtons m_buttons;

    QColor m_iconColor;
    QTimer m_timer;
    int m_intervalMsec = -1;
    QString m_title;
    QString m_message;
    QString m_icon;
    ushort m_iconId;
    QPixmap m_pixmap;
};

#endif // NOTIFICATION_H

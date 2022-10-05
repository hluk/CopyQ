// SPDX-License-Identifier: GPL-3.0-or-later

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

    ~NotificationDaemon();

    Notification *createNotification(const QString &id = QString());
    Notification *findNotification(const QString &id);

    void setPosition(Position position);

    void setOffset(int horizontalPoints, int verticalPoints);

    void setMaximumSize(int maximumWidthPoints, int maximumHeightPoints);

    void updateNotificationWidgets();

    void setNotificationOpacity(qreal opacity);

    void setNotificationStyleSheet(const QString &styleSheet);

    void setIconColor(const QColor &color);

    void setNativeNotificationsEnabled(bool enable) { m_nativeNotificationsEnabled = enable; }

    void removeNotification(const QString &id);

signals:
    void notificationButtonClicked(const NotificationButton &button);

private:
    void onNotificationClose(Notification *notification);
    void doUpdateNotificationWidgets();

    struct NotificationData {
        QString id;
        Notification *notification;
    };

    Notification *findNotification(Notification *notification);

    int offsetX() const;
    int offsetY() const;

    Position m_position;
    QList<NotificationData> m_notifications;
    QColor m_iconColor;
    qreal m_opacity;
    int m_horizontalOffsetPoints;
    int m_verticalOffsetPoints;
    int m_maximumWidthPoints;
    int m_maximumHeightPoints;
    QString m_styleSheet;
    QTimer m_timerUpdate;
    bool m_nativeNotificationsEnabled = true;
};

#endif // NOTIFICATIONDAEMON_H

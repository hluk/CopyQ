// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

#include "gui/notificationbutton.h"

class KNotification;
class QWidget;

class Notification : public QObject
{
    Q_OBJECT

public:
    // Same values as KNotification::Urgency
    enum class Urgency {
        Default = -1,
        Low = 10,
        Normal = 50,
        High = 70,
        Critical = 90,
    };

    enum class Persistency {
        Default = -1,
        NonPersistent = 0,
        Persistent = 1,
    };

    explicit Notification(QObject *parent) : QObject(parent) {}
    virtual void setTitle(const QString &title) = 0;
    virtual void setMessage(const QString &msg, Qt::TextFormat format = Qt::PlainText) = 0;
    virtual void setPixmap(const QPixmap &pixmap) = 0;
    virtual void setIcon(const QString &icon) = 0;
    virtual void setIcon(ushort icon) = 0;
    virtual void setInterval(int msec) = 0;
    virtual void setOpacity(qreal opacity) = 0;
    virtual void setButtons(const NotificationButtons &buttons) = 0;
    virtual void setUrgency(Urgency urgency) = 0;
    virtual void setPersistency(Persistency persistency) = 0;
    virtual void adjust() = 0;
    virtual QWidget *widget() = 0;
    virtual void show() = 0;
    virtual void close() = 0;

signals:
    /** Emitted if notification needs to be closed. */
    void closeNotification(Notification *self);

    void buttonClicked(const NotificationButton &button);
};

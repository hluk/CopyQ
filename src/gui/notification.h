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

#pragma once

#include <QObject>

#include "gui/notificationbutton.h"

class KNotification;
class QWidget;

class Notification : public QObject
{
    Q_OBJECT

public:
    explicit Notification(QObject *parent) : QObject(parent) {}
    virtual void setTitle(const QString &title) = 0;
    virtual void setMessage(const QString &msg, Qt::TextFormat format = Qt::PlainText) = 0;
    virtual void setPixmap(const QPixmap &pixmap) = 0;
    virtual void setIcon(const QString &icon) = 0;
    virtual void setIcon(ushort icon) = 0;
    virtual void setInterval(int msec) = 0;
    virtual void setOpacity(qreal opacity) = 0;
    virtual void setButtons(const NotificationButtons &buttons) = 0;
    virtual void adjust() = 0;
    virtual QWidget *widget() = 0;
    virtual void show() = 0;
    virtual void close() = 0;

signals:
    /** Emitted if notification needs to be closed. */
    void closeNotification(Notification *self);

    void buttonClicked(const NotificationButton &button);
};

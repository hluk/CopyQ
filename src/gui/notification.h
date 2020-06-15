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

#include <QTimer>
#include <QWidget>

class QLabel;
class QGridLayout;
class QHBoxLayout;

class Notification final : public QWidget
{
    Q_OBJECT

public:
    Notification();

    void setTitle(const QString &title);
    void setMessage(const QString &msg, Qt::TextFormat format = Qt::AutoText);
    void setPixmap(const QPixmap &pixmap);
    void setIcon(const QString &icon);
    void setIcon(ushort icon);
    void setInterval(int msec);
    void setOpacity(qreal opacity);
    void setButtons(const NotificationButtons &buttons);

    void updateIcon();

    void adjust();

    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

signals:
    /** Emitted if notification needs to be closed. */
    void closeNotification(Notification *self);

    void buttonClicked(const NotificationButton &button);

private:
    void onTimeout();
    void onButtonClicked(const NotificationButton &button);

    QGridLayout *m_layout = nullptr;
    QHBoxLayout *m_buttonLayout = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_iconLabel = nullptr;
    QLabel *m_msgLabel = nullptr;
    QTimer m_timer;
    bool m_autoclose = false;
    qreal m_opacity = 1.0;
    QString m_icon;
};

#endif // NOTIFICATION_H

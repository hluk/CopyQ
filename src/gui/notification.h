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

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <QTimer>
#include <QWidget>

class NotificationDaemon;
class QLabel;

class Notification : public QWidget
{
    Q_OBJECT
    friend class NotificationDaemon;
protected:
    explicit Notification(int id);

    void setTitle(const QString &title);
    void setMessage(const QString &msg, Qt::TextFormat format = Qt::PlainText);
    void setPixmap(const QPixmap &pixmap);
    void setIcon(ushort icon);
    void setInterval(int msec);
    void setOpacity(qreal opacity);
    void setClickToShowEnabled(bool enabled);

    void updateIcon();

    int id() const { return m_id; }

    void adjust();

    void mousePressEvent(QMouseEvent *event);
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);

signals:
    /** Emitted if notification needs to be closed. */
    void closeNotification(Notification *self);

private slots:
    void onTimeout();

private:
    const int m_id;
    QWidget *m_body;
    QLabel *m_titleLabel;
    QLabel *m_iconLabel;
    QLabel *m_msgLabel;
    QLabel *m_tipLabel;
    QTimer m_timer;
    qreal m_opacity;
    ushort m_icon;
};

#endif // NOTIFICATION_H

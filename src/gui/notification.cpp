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

#include "gui/notification.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMap>
#include <QTimer>
#include <QVBoxLayout>

Notification::Notification(QWidget *parent)
    : QWidget()
    , m_body(NULL)
    , m_titleLabel(NULL)
    , m_iconLabel(NULL)
    , m_msgLabel(NULL)
    , m_timer(NULL)
    , m_opacity(0.5)
{
    connect( parent, SIGNAL(destroyed()),
             this, SLOT(deleteLater()) );

    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet("* {color: #ddd; background: black}"
                  "#NotificationBody {border-radius: 0px}");

    QVBoxLayout *bodyLayout = new QVBoxLayout(this);
    m_body = new QWidget(this);
    m_body->setObjectName("NotificationBody");
    m_body->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    bodyLayout->addWidget(m_body);

    QGridLayout *layout = new QGridLayout(m_body);
    layout->setMargin(10);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    layout->addWidget(m_titleLabel, 0, 0, 1, 2, Qt::AlignCenter);
    m_titleLabel->setTextFormat(Qt::PlainText);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    layout->addWidget(m_iconLabel, 1, 0);

    m_msgLabel = new QLabel(this);
    m_msgLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    layout->addWidget(m_msgLabel, 1, 1);
    m_msgLabel->setTextFormat(Qt::PlainText);

    setWindowFlags(Qt::ToolTip);
    setWindowOpacity(m_opacity);
}

void Notification::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
    if (title.isEmpty()) {
        m_titleLabel->hide();
    } else {
        m_titleLabel->show();
        m_titleLabel->adjustSize();
    }
}

void Notification::setMessage(const QString &msg)
{
    m_msgLabel->setText(msg);
    m_msgLabel->adjustSize();
}

void Notification::setIcon(const QPixmap &icon)
{
    m_iconLabel->setPixmap(icon);
    m_iconLabel->resize(icon.size());
}

void Notification::setInterval(int msec)
{
    delete m_timer;
    m_timer = NULL;

    if (msec > 0) {
        m_timer = new QTimer(this);
        m_timer->setInterval(msec);
        connect( m_timer, SIGNAL(timeout()),
                 this, SLOT(deleteLater()) );
        m_timer->start();
    }
}

void Notification::mousePressEvent(QMouseEvent *)
{
    deleteLater();
}

void Notification::enterEvent(QEvent *event)
{
    setWindowOpacity(1.0);
    QWidget::enterEvent(event);
}

void Notification::leaveEvent(QEvent *event)
{
    setWindowOpacity(m_opacity);
    QWidget::leaveEvent(event);
}

void Notification::adjust()
{
    m_body->adjustSize();
    adjustSize();
}

void Notification::popup(const QPoint &position, int msec)
{
    move(position);

    const QRect r = rect();
    const int radius = 0;
    const int m = layout()->margin();
    const int w = r.width() - 2 * m;
    const int h = r.height() - 2 * m;
    const int x = r.x() + m;
    const int y = r.y() + m;
    const QRegion corner(x, y, radius * 2, radius * 2, QRegion::Ellipse);

    QRegion mask = corner;
    mask = mask.united( QRect(x + radius, y, w - radius * 2, h) );
    mask = mask.united( QRect(x, y + radius, w, h - radius * 2) );
    mask = mask.united( corner.translated(w - radius * 2, 0) );
    mask = mask.united( corner.translated(w - radius * 2, h - radius * 2) );
    mask = mask.united( corner.translated(0, h - radius * 2) );

    setMask(mask);

    show();
    setInterval(msec);
}

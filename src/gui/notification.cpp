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

    QVBoxLayout *bodyLayout = new QVBoxLayout(this);
    bodyLayout->setMargin(8);
    m_body = new QWidget(this);
    bodyLayout->addWidget(m_body);
    bodyLayout->setSizeConstraint(QLayout::SetMaximumSize);

    QGridLayout *layout = new QGridLayout(m_body);
    layout->setMargin(0);

    m_titleLabel = new QLabel(this);
    layout->addWidget(m_titleLabel, 0, 0, 1, 2, Qt::AlignCenter);
    m_titleLabel->setTextFormat(Qt::PlainText);

    m_iconLabel = new QLabel(this);
    layout->addWidget(m_iconLabel, 1, 0, Qt::AlignTop);

    m_msgLabel = new QLabel(this);
    layout->addWidget(m_msgLabel, 1, 1, Qt::AlignAbsolute);
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

void Notification::setOpacity(qreal opacity)
{
    setWindowOpacity(opacity);
    m_opacity = opacity;
}

void Notification::mousePressEvent(QMouseEvent *)
{
    deleteLater();
}

void Notification::enterEvent(QEvent *event)
{
    setWindowOpacity(1.0);
    if (m_timer != NULL)
        m_timer->stop();
    QWidget::enterEvent(event);
}

void Notification::leaveEvent(QEvent *event)
{
    setWindowOpacity(m_opacity);
    if (m_timer != NULL)
        m_timer->start();
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
    show();
    setInterval(msec);
}

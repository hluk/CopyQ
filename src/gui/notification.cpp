/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "common/appconfig.h"
#include "common/common.h"
#include "common/display.h"
#include "common/textdata.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <memory>

namespace {

class NotificationButtonWidget : public QPushButton
{
    Q_OBJECT

public:
    NotificationButtonWidget(const NotificationButton &button, QWidget *parent)
        : QPushButton(button.name, parent)
        , m_button(button)
    {
        connect( this, SIGNAL(clicked()),
                 this, SLOT(onClicked()) );
    }

signals:
    void clicked(const NotificationButton &button);

private slots:
    void onClicked()
    {
        emit clicked(m_button);
    }

private:
    NotificationButton m_button;
};

} // namespace

Notification::Notification(const QString &id, const QString &title, const NotificationButtons &buttons)
    : m_id(id)
    , m_body(nullptr)
    , m_titleLabel(nullptr)
    , m_iconLabel(nullptr)
    , m_msgLabel(nullptr)
    , m_opacity(1.0)
{
    auto bodyLayout = new QVBoxLayout(this);
    bodyLayout->setMargin(8);
    m_body = new QWidget(this);
    bodyLayout->addWidget(m_body);
    bodyLayout->setSizeConstraint(QLayout::SetMaximumSize);

    auto layout = new QGridLayout(m_body);
    layout->setMargin(0);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    m_msgLabel = new QLabel(this);

    if ( !title.isEmpty() ) {
        m_titleLabel = new QLabel(this);
        m_titleLabel->setObjectName("NotificationTitle");
        m_titleLabel->setTextFormat(Qt::PlainText);
        m_titleLabel->setText(title);

        layout->addWidget(m_iconLabel, 0, 0);
        layout->addWidget(m_titleLabel, 0, 1, Qt::AlignCenter);
        layout->addWidget(m_msgLabel, 1, 0, 1, 2);
    } else {
        layout->addWidget(m_iconLabel, 0, 0, Qt::AlignTop);
        layout->addWidget(m_msgLabel, 0, 1);
    }


    if ( !buttons.isEmpty() ) {
        auto buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        layout->addLayout(buttonLayout, 2, 0, 1, 2);

        for (const auto &button : buttons) {
            const auto buttonWidget = new NotificationButtonWidget(button, this);
            connect( buttonWidget, SIGNAL(clicked(NotificationButton)),
                     this, SLOT(onButtonClicked(NotificationButton)) );
            buttonLayout->addWidget(buttonWidget);
        }
    }

    setWindowFlags(Qt::ToolTip);
    setWindowOpacity(m_opacity);
}

void Notification::setMessage(const QString &msg, Qt::TextFormat format)
{
    m_msgLabel->setTextFormat(format);
    m_msgLabel->setText(msg);
    m_msgLabel->setVisible( !msg.isEmpty() );
}

void Notification::setPixmap(const QPixmap &pixmap)
{
    m_msgLabel->setPixmap(pixmap);
}

void Notification::setIcon(const QString &icon)
{
    m_icon = icon;
}

void Notification::setInterval(int msec)
{
    if (msec >= 0) {
        initSingleShotTimer( &m_timer, msec, this, SLOT(onTimeout()) );
        m_timer.start();
    } else {
        m_timer.stop();
    }
}

void Notification::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    setWindowOpacity(m_opacity);
}

void Notification::updateIcon()
{
    const QColor color = getDefaultIconColor(*this);
    const auto height = static_cast<int>( m_msgLabel->fontMetrics().lineSpacing() * 1.2 );
    const auto iconId = toIconId(m_icon);
    const auto pixmap = iconId == 0
            ? QPixmap(m_icon)
            : createPixmap(iconId, color, height);
    m_iconLabel->setPixmap(pixmap);
    m_iconLabel->resize(pixmap.size());
}

void Notification::adjust()
{
    m_body->adjustSize();
    adjustSize();
}

void Notification::mousePressEvent(QMouseEvent *)
{
    m_timer.stop();

    emit closeNotification(this);
}

void Notification::enterEvent(QEvent *event)
{
    setWindowOpacity(1.0);
    m_timer.stop();
    QWidget::enterEvent(event);
}

void Notification::leaveEvent(QEvent *event)
{
    setWindowOpacity(m_opacity);
    if ( m_timer.interval() > 0 )
        m_timer.start();
    QWidget::leaveEvent(event);
}

void Notification::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter p(this);

    // black outer border
    p.setPen(Qt::black);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // light inner border
    p.setPen( palette().color(QPalette::Window).lighter(300) );
    p.drawRect(rect().adjusted(1, 1, -2, -2));
}

void Notification::showEvent(QShowEvent *event)
{
    // QTBUG-33078: Window opacity must be set after show event.
    setWindowOpacity(m_opacity);
    QWidget::showEvent(event);
}

void Notification::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit closeNotification(this);
}

void Notification::onTimeout()
{
    emit closeNotification(this);
}

void Notification::onButtonClicked(const NotificationButton &button)
{
    emit buttonClicked(button);
    emit closeNotification(this);
}

#include "notification.moc"

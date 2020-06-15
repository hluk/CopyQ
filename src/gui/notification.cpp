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

#include "gui/notification.h"

#include "common/common.h"
#include "common/display.h"
#include "common/textdata.h"
#include "common/timer.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/pixelratio.h"

#include <QApplication>
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

#include <memory>

namespace {

class NotificationButtonWidget final : public QPushButton
{
    Q_OBJECT

public:
    NotificationButtonWidget(const NotificationButton &button, QWidget *parent)
        : QPushButton(button.name, parent)
        , m_button(button)
    {
        connect( this, &NotificationButtonWidget::clicked,
                 this, &NotificationButtonWidget::onClicked );
    }

signals:
    void clickedButton(const NotificationButton &button);

private:
    void onClicked()
    {
        emit clickedButton(m_button);
    }

    NotificationButton m_button;
};

} // namespace

Notification::Notification()
{
    m_layout = new QGridLayout(this);
    m_layout->setMargin(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    m_msgLabel = new QLabel(this);
    m_msgLabel->setAlignment(Qt::AlignTop | Qt::AlignAbsolute);

    setTitle(QString());

    setWindowFlags(Qt::ToolTip);
    setWindowOpacity(m_opacity);
    setAttribute(Qt::WA_ShowWithoutActivating);

    initSingleShotTimer( &m_timer, 0, this, &Notification::onTimeout );
}

void Notification::setTitle(const QString &title)
{
    if ( !title.isEmpty() ) {
        if (!m_titleLabel)
            m_titleLabel = new QLabel(this);

        m_titleLabel->setObjectName("NotificationTitle");
        m_titleLabel->setTextFormat(Qt::PlainText);
        m_titleLabel->setText(title);

        m_layout->addWidget(m_iconLabel, 0, 0);
        m_layout->addWidget(m_titleLabel, 0, 1, Qt::AlignCenter);
        m_layout->addWidget(m_msgLabel, 1, 0, 1, 2);
    } else {
        if (m_titleLabel) {
            m_titleLabel->deleteLater();
            m_titleLabel = nullptr;
        }

        m_layout->addWidget(m_iconLabel, 0, 0, Qt::AlignTop);
        m_layout->addWidget(m_msgLabel, 0, 1);
    }
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

void Notification::setIcon(ushort icon)
{
    m_icon = QString(QChar(icon));
}

void Notification::setInterval(int msec)
{
    if (msec >= 0) {
        m_autoclose = true;
        m_timer.setInterval(msec);
        if (isVisible())
            m_timer.start();
    } else {
        m_autoclose = false;
    }
}

void Notification::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    setWindowOpacity(m_opacity);
}

void Notification::setButtons(const NotificationButtons &buttons)
{
    for (const auto &buttonWidget : findChildren<NotificationButtonWidget*>())
        buttonWidget->deleteLater();

    if ( !buttons.isEmpty() ) {
        if (!m_buttonLayout)
            m_buttonLayout = new QHBoxLayout();

        m_buttonLayout->addStretch();
        m_layout->addLayout(m_buttonLayout, 2, 0, 1, 2);

        for (const auto &button : buttons) {
            const auto buttonWidget = new NotificationButtonWidget(button, this);
            connect( buttonWidget, &NotificationButtonWidget::clickedButton,
                     this, &Notification::onButtonClicked );
            m_buttonLayout->addWidget(buttonWidget);
        }
    } else if (m_buttonLayout) {
        m_buttonLayout->deleteLater();
        m_buttonLayout = nullptr;
    }
}

void Notification::updateIcon()
{
    const QColor color = getDefaultIconColor(*this);
    const auto height = static_cast<int>( m_msgLabel->fontMetrics().lineSpacing() * 1.2 );
    const auto iconId = toIconId(m_icon);

    const auto ratio = pixelRatio(this);

    auto pixmap = iconId == 0
            ? QPixmap(m_icon)
            : createPixmap(iconId, color, height * ratio);

    pixmap.setDevicePixelRatio(ratio);

    m_iconLabel->setPixmap(pixmap);
    m_iconLabel->resize(pixmap.size());
}

void Notification::adjust()
{
    m_msgLabel->setMaximumSize(maximumSize());
    if ( !m_msgLabel->isVisible() && m_msgLabel->sizeHint().width() > maximumWidth() ) {
        m_msgLabel->setWordWrap(true);
        m_msgLabel->adjustSize();
    }
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
    m_timer.start();

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
    if (m_autoclose)
        emit closeNotification(this);
}

void Notification::onButtonClicked(const NotificationButton &button)
{
    emit buttonClicked(button);
    emit closeNotification(this);
}

#include "notification.moc"

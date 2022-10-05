// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui/notificationbasic.h"
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

class NotificationBasic;

class NotificationBasicWidget final : public QWidget
{
    Q_OBJECT

public:
    NotificationBasicWidget(NotificationBasic *parent);

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
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void onTimeout();
    void onButtonClicked(const NotificationButton &button);

    NotificationBasic *m_parent;
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

class NotificationBasic final : public Notification
{
    Q_OBJECT

    friend class NotificationBasicWidget;

public:
    NotificationBasic(QObject *parent)
        : Notification(parent)
        , m_widget(this)
    {
        m_widget.setObjectName("Notification");
    }

    void setTitle(const QString &title) override {
        m_widget.setTitle(title);
    }
    void setMessage(const QString &msg, Qt::TextFormat format = Qt::AutoText) override {
        m_widget.setMessage(msg, format);
    }
    void setPixmap(const QPixmap &pixmap) override {
        m_widget.setPixmap(pixmap);
    }
    void setIcon(const QString &icon) override {
        m_widget.setIcon(icon);
    }
    void setIcon(ushort icon) override {
        m_widget.setIcon(icon);
    }
    void setInterval(int msec) override {
        m_widget.setInterval(msec);
    }
    void setOpacity(qreal opacity) override {
        m_widget.setOpacity(opacity);
    }
    void setButtons(const NotificationButtons &buttons) override {
        m_widget.setButtons(buttons);
    }

    void adjust() override {
        m_widget.updateIcon();
        m_widget.adjust();
    }

    QWidget *widget() override {
        m_widget.updateIcon();
        m_widget.adjust();
        return &m_widget;
    }

    void show() override {
        m_widget.show();
    }

    void close() override {
        m_widget.close();
    }

private:
    NotificationBasicWidget m_widget;
};

} // namespace

NotificationBasicWidget::NotificationBasicWidget(NotificationBasic *parent)
    : m_parent(parent)
{
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins({8,8,8,8});

    m_iconLabel = new QLabel(this);
    m_iconLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    m_msgLabel = new QLabel(this);
    m_msgLabel->setAlignment(Qt::AlignTop | Qt::AlignAbsolute);

    setTitle(QString());

    setWindowFlags(Qt::ToolTip);
    setWindowOpacity(m_opacity);
    setAttribute(Qt::WA_ShowWithoutActivating);

    initSingleShotTimer( &m_timer, 0, this, &NotificationBasicWidget::onTimeout );
}

void NotificationBasicWidget::setTitle(const QString &title)
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

void NotificationBasicWidget::setMessage(const QString &msg, Qt::TextFormat format)
{
    m_msgLabel->setTextFormat(format);
    m_msgLabel->setText(msg);
    m_msgLabel->setVisible( !msg.isEmpty() );
}

void NotificationBasicWidget::setPixmap(const QPixmap &pixmap)
{
    m_msgLabel->setPixmap(pixmap);
}

void NotificationBasicWidget::setIcon(const QString &icon)
{
    m_icon = icon;
}

void NotificationBasicWidget::setIcon(ushort icon)
{
    m_icon = QString(QChar(icon));
}

void NotificationBasicWidget::setInterval(int msec)
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

void NotificationBasicWidget::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    setWindowOpacity(m_opacity);
}

void NotificationBasicWidget::setButtons(const NotificationButtons &buttons)
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
                     this, &NotificationBasicWidget::onButtonClicked );
            m_buttonLayout->addWidget(buttonWidget);
        }
    } else if (m_buttonLayout) {
        m_buttonLayout->deleteLater();
        m_buttonLayout = nullptr;
    }
}

void NotificationBasicWidget::updateIcon()
{
    const QColor color = getDefaultIconColor(*this);
    const auto height = static_cast<int>( m_msgLabel->fontMetrics().lineSpacing() * 1.2 );
    const auto iconId = toIconId(m_icon);

    const auto ratio = pixelRatio(this);

    auto pixmap = iconId == 0
            ? QPixmap(m_icon)
            : createPixmap(iconId, color, static_cast<int>(height * ratio));

    pixmap.setDevicePixelRatio(ratio);

    m_iconLabel->setPixmap(pixmap);
    m_iconLabel->resize(pixmap.size());
}

void NotificationBasicWidget::adjust()
{
    m_msgLabel->setMaximumSize(maximumSize());
    if ( !m_msgLabel->isVisible() && m_msgLabel->sizeHint().width() > maximumWidth() ) {
        m_msgLabel->setWordWrap(true);
        m_msgLabel->adjustSize();
    }
    adjustSize();
}

void NotificationBasicWidget::mousePressEvent(QMouseEvent *)
{
    m_timer.stop();

    emit m_parent->closeNotification(m_parent);
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
void NotificationBasicWidget::enterEvent(QEnterEvent *event)
#else
void NotificationBasicWidget::enterEvent(QEvent *event)
#endif
{
    setWindowOpacity(1.0);
    m_timer.stop();
    QWidget::enterEvent(event);
}

void NotificationBasicWidget::leaveEvent(QEvent *event)
{
    setWindowOpacity(m_opacity);
    m_timer.start();
    QWidget::leaveEvent(event);
}

void NotificationBasicWidget::paintEvent(QPaintEvent *event)
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

void NotificationBasicWidget::showEvent(QShowEvent *event)
{
    m_timer.start();
    QWidget::showEvent(event);
}

void NotificationBasicWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit m_parent->closeNotification(m_parent);
}

void NotificationBasicWidget::onTimeout()
{
    if (m_autoclose)
        emit m_parent->closeNotification(m_parent);
}

void NotificationBasicWidget::onButtonClicked(const NotificationButton &button)
{
    emit m_parent->buttonClicked(button);
    emit m_parent->closeNotification(m_parent);
}

Notification *createNotificationBasic(QObject *parent)
{
    return new NotificationBasic(parent);
}

#include "notificationbasic.moc"

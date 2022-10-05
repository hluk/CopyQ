// SPDX-License-Identifier: GPL-3.0-or-later

#include "notificationnative.h"

#include "common/log.h"
#include "common/timer.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/notification.h"

#include <KNotification>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextEdit>

#include <knotifications_version.h>

#include <memory>

namespace {

constexpr auto componentName = "copyq";

QPixmap defaultIcon()
{
    static QPixmap pixmap = appIcon().pixmap(512);
    return pixmap;
}

class NotificationNative final : public Notification
{
    Q_OBJECT

public:
    explicit NotificationNative(const QColor &iconColor, QObject *parent = nullptr);

    ~NotificationNative();

    void setTitle(const QString &title) override;
    void setMessage(const QString &msg, Qt::TextFormat format = Qt::PlainText) override;
    void setPixmap(const QPixmap &pixmap) override;
    void setIcon(const QString &icon) override;
    void setIcon(ushort icon) override;
    void setInterval(int msec) override;
    void setOpacity(qreal) override {}
    void setButtons(const NotificationButtons &buttons) override;
    void adjust() override {}
    QWidget *widget() override { return nullptr; }
    void show() override;
    void close() override;

private:
    void onButtonClicked(unsigned int id);
    void onDestroyed();
    void onClosed();
    void onIgnored();
    void onActivated();
    void update();

    void notificationLog(const char *message);

    KNotification *dropNotification();

    QPointer<KNotification> m_notification;
    NotificationButtons m_buttons;

    QColor m_iconColor;
    QTimer m_timer;
    int m_intervalMsec = -1;
    QString m_title;
    QString m_message;
    QString m_icon;
    ushort m_iconId;
    QPixmap m_pixmap;
    bool m_closed = false;
};

} // namespace

NotificationNative::NotificationNative(const QColor &iconColor, QObject *parent)
    : Notification(parent)
    , m_iconColor(iconColor)
{
    initSingleShotTimer( &m_timer, 0, this, &NotificationNative::close );
}

NotificationNative::~NotificationNative()
{
    auto notification = dropNotification();
    if (notification) {
        notificationLog("Delete");
        notification->close();
    }
}

void NotificationNative::setTitle(const QString &title)
{
    m_title = title;
}

void NotificationNative::setMessage(const QString &msg, Qt::TextFormat format)
{
    if (format == Qt::PlainText)
        m_message = msg.toHtmlEscaped();
    else
        m_message = msg;
}

void NotificationNative::setPixmap(const QPixmap &pixmap)
{
    m_icon.clear();
    m_iconId = 0;
    m_pixmap = pixmap;
}

void NotificationNative::setIcon(const QString &icon)
{
    m_iconId = toIconId(icon);
    if (m_iconId == 0)
        m_icon = icon;
    else
        m_icon.clear();
    m_pixmap = QPixmap();
}

void NotificationNative::setIcon(ushort icon)
{
    m_icon.clear();
    m_iconId = icon;
    m_pixmap = QPixmap();
}

void NotificationNative::setInterval(int msec)
{
    m_intervalMsec = msec;
}

void NotificationNative::setButtons(const NotificationButtons &buttons)
{
    m_buttons = buttons;
}

void NotificationNative::show()
{
    if (m_closed)
        return;

    notificationLog("Update");

    if (m_notification) {
        update();
        if (m_notification)
            m_notification->update();
        notificationLog("Updated");
        return;
    }

    m_notification = new KNotification("generic");
    notificationLog("Create");
    m_notification->setComponentName(componentName);

    connect( m_notification.data(), static_cast<void (KNotification::*)(unsigned int)>(&KNotification::activated),
             this, &NotificationNative::onButtonClicked );
    connect( m_notification.data(), &KNotification::closed,
             this, &NotificationNative::onClosed );
    connect( m_notification.data(), &KNotification::ignored,
             this, &NotificationNative::onIgnored );
#if KNOTIFICATIONS_VERSION < QT_VERSION_CHECK(5,67,0)
    connect( m_notification.data(), static_cast<void (KNotification::*)()>(&KNotification::activated),
             this, &NotificationNative::onActivated );
#else
    connect( m_notification.data(), &KNotification::defaultActivated,
             this, &NotificationNative::onActivated );
#endif
    connect( m_notification.data(), &QObject::destroyed,
             this, &NotificationNative::onDestroyed );

    update();
    if (m_notification)
        m_notification->sendEvent();

    notificationLog("Created");
}

void NotificationNative::close()
{
    notificationLog("Close");

    auto notification = dropNotification();
    if (notification)
        notification->close();

    notificationLog("Closed");

    emit closeNotification(this);
}

void NotificationNative::onButtonClicked(unsigned int id)
{
    notificationLog("onButtonClicked");

    if ( id - 1 < static_cast<unsigned int>(m_buttons.size()) ) {
        emit buttonClicked(m_buttons[id - 1]);
        emit closeNotification(this);
    }
}

void NotificationNative::onDestroyed()
{
    notificationLog("Destroyed");
    dropNotification();
}

void NotificationNative::onClosed()
{
    notificationLog("onClosed");
    dropNotification();
}

void NotificationNative::onIgnored()
{
    notificationLog("onIgnored");
    dropNotification();
}

void NotificationNative::onActivated()
{
    notificationLog("onActivated");
    dropNotification();
}

void NotificationNative::update()
{
    if (!m_notification)
        return;

    if (m_intervalMsec == 0) {
        close();
        return;
    }

    m_notification->setTitle(m_title);
#ifdef Q_OS_WIN
    // On Windows, notification doesn't show up if the message is empty.
    if (m_message.isEmpty())
        m_notification->setText(QLatin1String("-"));
    else
        m_notification->setText(m_message);
#else
    m_notification->setText(m_message);
#endif

    if (m_pixmap.isNull() && m_iconId != 0) {
        const auto height = 64;
        const auto ratio = qApp->devicePixelRatio();
        m_pixmap = createPixmap(m_iconId, m_iconColor, static_cast<int>(height * ratio));
        m_pixmap.setDevicePixelRatio(ratio);
    }

    if ( !m_icon.isEmpty() ) {
        m_notification->setIconName(m_icon);
    } else if ( !m_pixmap.isNull() ) {
        m_notification->setPixmap(m_pixmap);
    } else {
        m_notification->setPixmap(defaultIcon());
    }

    QStringList actions;
    for (const auto &button : m_buttons)
        actions.append(button.name);
    m_notification->setActions(actions);

    if (m_intervalMsec < 0) {
        m_timer.stop();
        m_notification->setFlags(KNotification::Persistent);
#if KNOTIFICATIONS_VERSION >= QT_VERSION_CHECK(5,58,0)
        m_notification->setUrgency(KNotification::HighUrgency);
#endif
    } else {
        // Specific timeout is not supported by KNotifications.
        m_timer.start(m_intervalMsec);
        m_notification->setFlags(KNotification::CloseOnTimeout);
#if KNOTIFICATIONS_VERSION >= QT_VERSION_CHECK(5,58,0)
        const KNotification::Urgency urgency = m_intervalMsec <= 10000
            ? KNotification::LowUrgency
            : KNotification::NormalUrgency;
        m_notification->setUrgency(urgency);
#endif
    }
}

void NotificationNative::notificationLog(const char *message)
{
    COPYQ_LOG_VERBOSE(
        QString("Notification [%1:%2]: %3")
        .arg(reinterpret_cast<quintptr>(this))
        .arg(reinterpret_cast<quintptr>(m_notification.data()))
        .arg(message) );
}

KNotification *NotificationNative::dropNotification()
{
    m_closed = true;
    if (!m_notification)
        return nullptr;

    auto notification = m_notification;
    m_notification = nullptr;
    notification->disconnect(this);
    return notification;
}

Notification *createNotificationNative(const QColor &iconColor, QObject *parent)
{
    return new NotificationNative(iconColor, parent);
}

#include "notificationnative.moc"

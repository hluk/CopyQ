/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScopedPointer>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {

void showNotificationInspectDialog(const QString &messageTitle, const QString &message)
{
    QScopedPointer<QDialog> dialog(new QDialog);
    dialog->setObjectName("InspectNotificationDialog");
    dialog->setWindowTitle( Notification::tr("CopyQ Inspect Notification") );

    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->registerWindowGeometry( dialog.data() );

    if ( cm->value("always_on_top").toBool() )
        dialog->setWindowFlags( dialog->windowFlags() ^ Qt::WindowStaysOnTopHint );

    IconFactory *iconFactory = cm->iconFactory();
    dialog->setWindowIcon( iconFactory->appIcon() );

    QTextEdit *editor = new QTextEdit(dialog.data());
    editor->setReadOnly(true);
    editor->setText(
        "<h3>" + escapeHtml(messageTitle) + "</h3>"
        "<p>" + escapeHtml(message) + "</p>"
        );

    QDialogButtonBox *buttons = new QDialogButtonBox(
                QDialogButtonBox::Close, Qt::Horizontal, dialog.data() );
    QObject::connect( buttons, SIGNAL(rejected()), dialog.data(), SLOT(close()) );

    QPushButton *copyButton = new QPushButton( Notification::tr("&Copy"), buttons );
    const QColor color = getDefaultIconColor(*copyButton, QPalette::Window);
    const QIcon icon = iconFactory->getIcon("clipboard", IconPaste, color, color);
    copyButton->setIcon(icon);
    QObject::connect( copyButton, SIGNAL(clicked()), editor, SLOT(selectAll()) );
    QObject::connect( copyButton, SIGNAL(clicked()), editor, SLOT(copy()) );
    buttons->addButton(copyButton, QDialogButtonBox::ActionRole);

    QVBoxLayout *layout = new QVBoxLayout( dialog.data() );
    layout->addWidget(editor);
    layout->addWidget(buttons);

    dialog->adjustSize();
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog.take()->show();
}

} // namespace

Notification::Notification(int id)
    : m_id(id)
    , m_body(NULL)
    , m_titleLabel(NULL)
    , m_iconLabel(NULL)
    , m_msgLabel(NULL)
    , m_timer(NULL)
    , m_opacity(1.0)
    , m_icon(0)
{
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
    setTitle( QString() );

    m_iconLabel = new QLabel(this);
    layout->addWidget(m_iconLabel, 1, 0, Qt::AlignTop);

    m_msgLabel = new QLabel(this);
    layout->addWidget(m_msgLabel, 1, 1, Qt::AlignAbsolute);

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

void Notification::setMessage(const QString &msg, Qt::TextFormat format)
{
    m_msgLabel->setTextFormat(format);
    m_msgLabel->setText(msg);
    m_msgLabel->adjustSize();

    m_textToCopy = (format == Qt::PlainText) ? msg : QString();

}

void Notification::setPixmap(const QPixmap &pixmap)
{
    m_msgLabel->setPixmap(pixmap);
}

void Notification::setIcon(ushort icon)
{
    m_icon = icon;
}

void Notification::setInterval(int msec)
{
    delete m_timer;
    m_timer = NULL;

    if (msec >= 0) {
        m_timer = new QTimer(this);
        m_timer->setInterval(msec);
        m_timer->setSingleShot(true);
        connect( m_timer, SIGNAL(timeout()),
                 this, SLOT(onTimeout()) );
        m_timer->start();
    }
}

void Notification::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    setWindowOpacity(m_opacity);
}

void Notification::updateIcon()
{
    const QColor color = getDefaultIconColor(*this, QPalette::Window);
    IconFactory *iconFactory = ConfigurationManager::instance()->iconFactory();
    const int height = m_msgLabel->fontMetrics().lineSpacing() * 1.2;
    const QPixmap pixmap = iconFactory->createPixmap(m_icon, color, height);
    m_iconLabel->setPixmap(pixmap);
    m_iconLabel->resize(pixmap.size());
}

void Notification::adjust()
{
    m_body->adjustSize();
    adjustSize();
}

void Notification::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        if ( !m_textToCopy.isEmpty() )
            showNotificationInspectDialog(m_titleLabel->text(), m_textToCopy);
        return;
    }

    if (m_timer != NULL)
        m_timer->stop();

    emit closeNotification(this);
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

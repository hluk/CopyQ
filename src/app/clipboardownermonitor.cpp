// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardownermonitor.h"

#include "common/appconfig.h"
#include "platform/platformwindow.h"

#include <QCoreApplication>

ClipboardOwnerMonitor::ClipboardOwnerMonitor()
{
    qApp->installNativeEventFilter(this);

    m_timer.setSingleShot(true);
    const int delay = AppConfig().option<Config::change_clipboard_owner_delay_ms>();
    m_timer.setInterval(delay);
    QObject::connect( &m_timer, &QTimer::timeout, [this]() {
        m_clipboardOwner = m_newClipboardOwner;

        PlatformWindowPtr currentWindow = platformNativeInterface()->getCurrentWindow();
        if (currentWindow) {
            const auto currentWindowTitle = currentWindow->getTitle().toUtf8();
            if (m_newClipboardOwner != currentWindowTitle) {
                m_newClipboardOwner = currentWindowTitle;
                m_timer.start();
            }
        }
    });
}

ClipboardOwnerMonitor::~ClipboardOwnerMonitor()
{
    qApp->removeNativeEventFilter(this);
}

bool ClipboardOwnerMonitor::nativeEventFilter(const QByteArray &, void *, NativeEventResult *)
{
    if ( !m_timer.isActive() )
        m_timer.start();

    return false;
}

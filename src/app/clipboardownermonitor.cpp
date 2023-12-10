// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardownermonitor.h"

#include "app/clipboardmonitor.h"
#include "common/log.h"

#include <QCoreApplication>

constexpr int updateAfterEventIntervalMs = 20;

ClipboardOwnerMonitor::ClipboardOwnerMonitor(ClipboardMonitor *monitor)
    : m_monitor(monitor)
{
    qApp->installNativeEventFilter(this);

    m_timerSetOwner.setSingleShot(true);

    m_timerUpdateAfterEvent.setSingleShot(true);
    m_timerUpdateAfterEvent.setInterval(updateAfterEventIntervalMs);

    QObject::connect(
        &m_timerSetOwner, &QTimer::timeout,
        [this]() {
            if (!m_nextClipboardOwners.isEmpty()) {
                m_monitor->setClipboardOwner(m_nextClipboardOwners.takeFirst());
                if (!m_nextClipboardOwners.isEmpty())
                    m_timerSetOwner.start();
            }
        });

    QObject::connect( &m_timerUpdateAfterEvent, &QTimer::timeout, [this]() {
        const QString title = m_monitor->currentClipboardOwner();
        if (m_lastClipboardOwner != title) {
            m_lastClipboardOwner = title;
            if ( m_timerSetOwner.interval() == 0 )
                m_nextClipboardOwners = QStringList{m_lastClipboardOwner};
            else
                m_nextClipboardOwners.append(m_lastClipboardOwner);

            if (!m_timerSetOwner.isActive())
                m_timerSetOwner.start();

            COPYQ_LOG(QStringLiteral("Next clipboard owner: %1").arg(title));
        }
    });
}

ClipboardOwnerMonitor::~ClipboardOwnerMonitor()
{
    qApp->removeNativeEventFilter(this);
}

void ClipboardOwnerMonitor::update()
{
    if ( m_timerSetOwner.interval() == 0 ) {
        m_lastClipboardOwner = m_monitor->currentClipboardOwner();
        m_nextClipboardOwners.clear();
        m_monitor->setClipboardOwner(m_lastClipboardOwner);
    } else if ( !m_timerUpdateAfterEvent.isActive() ) {
        m_timerUpdateAfterEvent.start();
    }
}

bool ClipboardOwnerMonitor::nativeEventFilter(const QByteArray &, void *, NativeEventResult *)
{
    if ( !m_timerUpdateAfterEvent.isActive() )
        m_timerUpdateAfterEvent.start();

    return false;
}

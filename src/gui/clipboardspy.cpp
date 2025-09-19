// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardspy.h"

#include "common/common.h"
#include "common/mimetypes.h"

#include "platform/platformclipboard.h"
#include "platform/platformnativeinterface.h"

#include <QApplication>
#include <QTimer>

ClipboardSpy::ClipboardSpy(ClipboardMode mode, const QByteArray &owner)
    : m_mode(mode)
    , m_clipboard(platformNativeInterface()->clipboard())
{
    m_oldOwnerData = owner.isEmpty() ? currentOwnerData() : owner;

    // Disable checking the selection/clipboard unnecessarily
    m_clipboard->setMonitoringEnabled(
        mode == ClipboardMode::Clipboard ? ClipboardMode::Selection : ClipboardMode::Clipboard,
        false);
}

void ClipboardSpy::wait(int ms, int checkIntervalMs)
{
    if (m_mode == ClipboardMode::Selection && !m_clipboard->isSelectionSupported())
        return;

    QEventLoop loop;
    connect( this, &ClipboardSpy::changed, &loop, &QEventLoop::quit );
    connect( this, &ClipboardSpy::stopped, &loop, &QEventLoop::quit );

    QTimer timerStop;
    if (ms >= 0) {
        timerStop.setInterval(ms);
        connect( &timerStop, &QTimer::timeout, &loop, &QEventLoop::quit );
        timerStop.start();
    }

    QTimer timerCheck;
    timerCheck.setInterval(checkIntervalMs);
    connect( &timerCheck, &QTimer::timeout, this, &ClipboardSpy::emitChangeIfChanged );
    timerCheck.start();

    loop.exec();
}

bool ClipboardSpy::setClipboardData(const QVariantMap &data)
{
    m_settingClipboard = true;
    m_oldOwnerData = currentOwnerData();

    m_clipboard->startMonitoring( QStringList(mimeOwner) );
    connect(m_clipboard.get(), &PlatformClipboard::changed, this, &ClipboardSpy::emitChangeIfChanged);

    m_clipboard->setData(m_mode, data);
    wait();
    m_oldOwnerData = data.value(mimeOwner).toByteArray();
    m_settingClipboard = false;
    return m_oldOwnerData == currentOwnerData();
}

QByteArray ClipboardSpy::currentOwnerData() const
{
    return m_clipboard->data(m_mode, QStringList(mimeOwner)).value(mimeOwner).toByteArray();
}

void ClipboardSpy::stop()
{
    emit stopped();
}

void ClipboardSpy::emitChangeIfChanged()
{
    const auto newOwner = currentOwnerData();
    if (m_oldOwnerData != newOwner) {
        emit changed();
    } else if (!m_settingClipboard && m_oldOwnerData.isEmpty() && newOwner.isEmpty()) {
        emit changed();
    }
}

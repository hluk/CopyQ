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
    connect( m_clipboard.get(), &PlatformClipboard::changed, this,
        [this](ClipboardMode changedMode) {
            if (changedMode == m_mode)
                check();
        }
    );
    m_clipboard->startMonitoring( QStringList(mimeOwner) );
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
    connect( &timerCheck, &QTimer::timeout, this, &ClipboardSpy::check );
    timerCheck.start();

    loop.exec();
}

bool ClipboardSpy::setClipboardData(const QVariantMap &data)
{
    m_oldOwnerData = currentOwnerData();
    m_clipboard->setData(m_mode, data);
    wait();
    m_oldOwnerData = data.value(mimeOwner).toByteArray();
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

bool ClipboardSpy::check()
{
    if (!m_oldOwnerData.isEmpty() && m_oldOwnerData == currentOwnerData())
        return false;

    emit changed();
    return true;
}

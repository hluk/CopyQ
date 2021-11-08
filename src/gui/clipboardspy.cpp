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

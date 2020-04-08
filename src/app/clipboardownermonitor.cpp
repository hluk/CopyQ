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

#include "clipboardownermonitor.h"

#include "platform/platformwindow.h"

#include <QCoreApplication>

ClipboardOwnerMonitor::ClipboardOwnerMonitor()
{
    qApp->installNativeEventFilter(this);

    m_timer.setSingleShot(true);
    m_timer.setInterval(50);
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

bool ClipboardOwnerMonitor::nativeEventFilter(const QByteArray &, void *, long *)
{
    if ( !m_timer.isActive() )
        m_timer.start();

    return false;
}

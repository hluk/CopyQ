/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include "app.h"
#include "client.h"

#include "platform/platformnativeinterface.h"

/**
 * Monitors clipboard and sends new clipboard data to server.
 * Server can send back new data for clipboard.
 *
 * Only the monitor should change the clipboard content.
 *
 * After monitor is executed it needs to be configured by sending special data
 * packet containing configuration.
 */
class ClipboardMonitor : public Client, public App
{
    Q_OBJECT

public:
    ClipboardMonitor(int &argc, char **argv);

private slots:
    void onClipboardChanged(PlatformClipboard::Mode mode);

    void onMessageReceived(const QByteArray &message, int messageCode);

    void onDisconnected();

private:
    PlatformClipboardPtr m_clipboard;
    QStringList m_formats;
    QVariantMap m_lastData[3]; /// Last data sent for each clipboard mode
};

#endif // CLIPBOARDMONITOR_H

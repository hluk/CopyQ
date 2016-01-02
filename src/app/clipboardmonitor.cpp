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

#include "clipboardmonitor.h"

#include "common/log.h"
#include "common/mimetypes.h"
#include "common/monitormessagecode.h"
#include "item/serialize.h"
#include "platform/platformclipboard.h"

#include <QApplication>

namespace {

bool hasSameData(const QVariantMap &data, const QVariantMap &lastData)
{
    foreach (const QString &format, lastData.keys()) {
        if ( !format.startsWith(COPYQ_MIME_PREFIX)
             && !data.contains(format) )
        {
            return false;
        }
    }

    foreach (const QString &format, data.keys()) {
        if ( !format.startsWith(COPYQ_MIME_PREFIX)
             && !data[format].toByteArray().isEmpty()
             && data[format] != lastData.value(format) )
        {
            return false;
        }
    }

    return true;
}

} // namespace

ClipboardMonitor::ClipboardMonitor(int &argc, char **argv)
    : Client()
    , App(createPlatformNativeInterface()->createMonitorApplication(argc, argv))
    , m_clipboard(createPlatformNativeInterface()->clipboard())
{
    restoreSettings();

    Q_ASSERT(argc == 3);
    const QString serverName( QString::fromUtf8(argv[2]) );

#ifdef HAS_TESTS
    if ( serverName == QString("copyq_TEST") )
        qApp->setProperty("CopyQ_testing", true);
#endif

    if ( !startClientSocket(serverName, argc, argv) )
        exit(1);
}

void ClipboardMonitor::onClipboardChanged(PlatformClipboard::Mode mode)
{
    QVariantMap data = m_clipboard->data(mode, m_formats);
    QVariantMap &lastData = m_lastData[mode];

    if ( hasSameData(data, lastData) ) {
        COPYQ_LOG("Ignoring unchanged clipboard content");
        return;
    }

    if (mode != PlatformClipboard::Clipboard) {
        const QString modeName = mode == PlatformClipboard::Selection
                ? "selection"
                : "find buffer";
        data.insert(mimeClipboardMode, modeName);
    }

    // add window title of clipboard owner
    if ( !data.contains(mimeOwner) && !data.contains(mimeWindowTitle) ) {
        PlatformPtr platform = createPlatformNativeInterface();
        PlatformWindowPtr currentWindow = platform->getCurrentWindow();
        if (currentWindow)
            data.insert( mimeWindowTitle, currentWindow->getTitle().toUtf8() );
    }

    sendMessage( serializeData(data), MonitorClipboardChanged );
    lastData = data;
}

void ClipboardMonitor::onMessageReceived(const QByteArray &message, int messageCode)
{
    if (messageCode == MonitorPing) {
        sendMessage( QByteArray(), MonitorPong );
    } else if (messageCode == MonitorSettings) {
        QVariantMap settings;
        QDataStream stream(message);
        stream >> settings;

        if ( hasLogLevel(LogDebug) ) {
            COPYQ_LOG("Loading configuration:");
            foreach (const QString &key, settings.keys()) {
                const QVariant val = settings[key];
                const QString str = val.canConvert<QStringList>() ? val.toStringList().join(",")
                                                                  : val.toString();
                COPYQ_LOG( QString(" %1=%2").arg(key).arg(str) );
            }
        }

        if ( settings.contains("formats") )
            m_formats = settings["formats"].toStringList();

        connect( m_clipboard.data(), SIGNAL(changed(PlatformClipboard::Mode)),
                 this, SLOT(onClipboardChanged(PlatformClipboard::Mode)),
                 Qt::UniqueConnection );

        m_clipboard->loadSettings(settings);

        COPYQ_LOG("Configured");
    } else if (messageCode == MonitorChangeClipboard
            || messageCode == MonitorChangeSelection)
    {
        QVariantMap data;
        deserializeData(&data, message);
        if (messageCode == MonitorChangeClipboard)
            m_clipboard->setData(PlatformClipboard::Clipboard, data);
        if (messageCode == MonitorChangeSelection)
            m_clipboard->setData(PlatformClipboard::Selection, data);
    } else {
        log( QString("Unknown message code %1!").arg(messageCode), LogError );
    }
}

void ClipboardMonitor::onDisconnected()
{
    exit(0);
}

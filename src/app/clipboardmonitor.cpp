/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "common/action.h"
#include "common/appconfig.h"
#include "common/common.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/serialize.h"
#include "platform/platformclipboard.h"
#include "platform/platformwindow.h"

#include <QApplication>

namespace {

bool hasSameData(const QVariantMap &data, const QVariantMap &lastData)
{
    for (auto it = lastData.constBegin(); it != lastData.constEnd(); ++it) {
        const auto &format = it.key();
        if ( !format.startsWith(COPYQ_MIME_PREFIX)
             && !data.contains(format) )
        {
            return false;
        }
    }

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const auto &format = it.key();
        if ( !format.startsWith(COPYQ_MIME_PREFIX)
             && !data[format].toByteArray().isEmpty()
             && data[format] != lastData.value(format) )
        {
            return false;
        }
    }

    return true;
}

#ifdef HAS_MOUSE_SELECTIONS
bool needSyncClipboardToSelection(const QVariantMap &data)
{
    return isClipboardData(data)
            && AppConfig().option<Config::copy_clipboard>()
            && !clipboardContains(ClipboardMode::Selection, data);
}

bool needSyncSelectionToClipboard(const QVariantMap &data)
{
    return !isClipboardData(data)
            && AppConfig().option<Config::copy_selection>()
            && !clipboardContains(ClipboardMode::Clipboard, data);
}

bool needStore(const QVariantMap &data)
{
    return isClipboardData(data)
            ? AppConfig().option<Config::check_clipboard>()
            : AppConfig().option<Config::check_selection>();
}
#else
bool needSyncClipboardToSelection(const QVariantMap &)
{
    return false;
}

bool needSyncSelectionToClipboard(const QVariantMap &)
{
    return false;
}

bool needStore(const QVariantMap &data)
{
    return isClipboardData(data)
            && AppConfig().option<Config::check_clipboard>();
}
#endif

bool isClipboardDataHidden(const QVariantMap &data)
{
    return data.value(mimeHidden).toByteArray() == "1";
}

} // namespace

ClipboardMonitor::ClipboardMonitor(const QStringList &formats)
    : m_clipboard(createPlatformNativeInterface()->clipboard())
    , m_formats(formats)
{
    m_clipboard->setFormats(formats);
    connect( m_clipboard.get(), SIGNAL(changed(ClipboardMode)),
             this, SLOT(onClipboardChanged(ClipboardMode)) );
}

void ClipboardMonitor::onClipboardChanged(ClipboardMode mode)
{
    QVariantMap data = m_clipboard->data(mode, m_formats);
    auto clipboardData = mode == ClipboardMode::Clipboard
            ? &m_clipboardData : &m_selectionData;

    if ( hasSameData(data, clipboardData->lastData) ) {
        COPYQ_LOG( QString("Ignoring unchanged %1")
                   .arg(mode == ClipboardMode::Clipboard ? "clipboard" : "selection") );
        return;
    }

    COPYQ_LOG( QString("%1 changed")
               .arg(mode == ClipboardMode::Clipboard ? "Clipboard" : "Selection") );

    if (mode != ClipboardMode::Clipboard) {
        const QString modeName = mode == ClipboardMode::Selection
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

    clipboardData->lastData = data;
    clipboardData->runAutomaticCommands = true;
    runAutomaticCommands();
}

void ClipboardMonitor::runAutomaticCommands()
{
    if (m_executingAutomaticCommands)
        return;

    for (;;) {
        ClipboardData *clipboardData = nullptr;
        if (m_clipboardData.runAutomaticCommands) {
            clipboardData = &m_clipboardData;
        } else if (m_selectionData.runAutomaticCommands) {
            clipboardData = &m_selectionData;
        }

        if (clipboardData == nullptr)
            return;

        m_executingAutomaticCommands = true;

        clipboardData->runAutomaticCommands = false;
        auto &data = clipboardData->lastData;

        if ( ownsClipboardData(data) || isClipboardDataHidden(data) ) {
            emit runScriptRequest("updateTitle(); showDataNotification()", data);
        } else {
            setTextData(&data, defaultTabName(), mimeCurrentTab);

            if ( needStore(data) ) {
                const auto clipboardTab = AppConfig().option<Config::clipboard_tab>();
                setTextData(&data, clipboardTab, mimeOutputTab);
            }

            if ( needSyncClipboardToSelection(data) )
                data.insert(mimeSyncToSelection, QByteArray());

            if ( needSyncSelectionToClipboard(data) )
                data.insert(mimeSyncToClipboard, QByteArray());

            emit runScriptRequest("onClipboardChanged()", data);
        }

        m_executingAutomaticCommands = false;
    }
}

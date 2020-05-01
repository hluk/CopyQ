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

#include "clipboardmonitor.h"

#include "common/action.h"
#include "common/appconfig.h"
#include "common/common.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/serialize.h"
#include "platform/platformclipboard.h"

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

bool isClipboardDataHidden(const QVariantMap &data)
{
    return data.value(mimeHidden).toByteArray() == "1";
}

} // namespace

ClipboardMonitor::ClipboardMonitor(const QStringList &formats)
    : m_clipboard(platformNativeInterface()->clipboard())
    , m_formats(formats)
{
    const AppConfig config;
    m_storeClipboard = config.option<Config::check_clipboard>();
    m_clipboardTab = config.option<Config::clipboard_tab>();

    m_clipboard->startMonitoring(formats);
    connect( m_clipboard.get(), &PlatformClipboard::changed,
             this, &ClipboardMonitor::onClipboardChanged );

#ifdef HAS_MOUSE_SELECTIONS
    m_storeSelection = config.option<Config::check_selection>();
    m_runSelection = config.option<Config::run_selection>();

    m_clipboardToSelection = config.option<Config::copy_clipboard>();
    m_selectionToClipboard = config.option<Config::copy_selection>();

    if (!m_storeSelection && !m_runSelection && !m_selectionToClipboard) {
        COPYQ_LOG("Disabling selection monitoring");
        m_clipboard->setMonitoringEnabled(ClipboardMode::Selection, false);
    }

    onClipboardChanged(ClipboardMode::Selection);
#endif
    onClipboardChanged(ClipboardMode::Clipboard);
}

void ClipboardMonitor::onClipboardChanged(ClipboardMode mode)
{
    QVariantMap data = m_clipboard->data(mode, m_formats);
    auto clipboardData = mode == ClipboardMode::Clipboard
            ? &m_clipboardData : &m_selectionData;

    if ( hasSameData(data, *clipboardData) ) {
#ifdef HAS_MOUSE_SELECTIONS
        if ( !m_runSelection && mode == ClipboardMode::Selection )
            return;
#endif
        COPYQ_LOG( QString("Ignoring unchanged %1")
                   .arg(mode == ClipboardMode::Clipboard ? "clipboard" : "selection") );
        emit clipboardUnchanged(data);
        return;
    }

    *clipboardData = data;

    COPYQ_LOG( QString("%1 changed, owner is \"%2\"")
               .arg(mode == ClipboardMode::Clipboard ? "Clipboard" : "Selection",
                    getTextData(data, mimeOwner)) );

#ifdef HAS_MOUSE_SELECTIONS
    if ( (mode == ClipboardMode::Clipboard ? m_clipboardToSelection : m_selectionToClipboard)
        && !data.contains(mimeOwner) )
    {
        const auto text = getTextData(data);
        if ( !text.isEmpty() ) {
            const auto targetMode = mode == ClipboardMode::Clipboard
                ? ClipboardMode::Selection
                : ClipboardMode::Clipboard;
            const QVariantMap targetData = m_clipboard->data(targetMode, QStringList(mimeText));
            const uint targetTextHash = qHash( getTextData(targetData, mimeText) );
            emit synchronizeSelection(mode, text, targetTextHash);
        }
    }

    // omit running run automatic commands when disabled
    if ( !m_runSelection && mode == ClipboardMode::Selection )
        return;
#endif

    if (mode != ClipboardMode::Clipboard) {
        const QString modeName = mode == ClipboardMode::Selection
                ? "selection"
                : "find buffer";
        data.insert(mimeClipboardMode, modeName);
    }

    // add window title of clipboard owner
    if ( !data.contains(mimeOwner) && !data.contains(mimeWindowTitle) ) {
        const QByteArray windowTitle = m_clipboard->clipboardOwner();
        if ( !windowTitle.isEmpty() )
            data.insert(mimeWindowTitle, windowTitle);
    }

    // run automatic commands
    if ( anySessionOwnsClipboardData(data) ) {
        emit clipboardChanged(data, ClipboardOwnership::Own);
    } else if ( isClipboardDataHidden(data) ) {
        emit clipboardChanged(data, ClipboardOwnership::Hidden);
    } else {
        const auto defaultTab = m_clipboardTab.isEmpty() ? defaultClipboardTabName() : m_clipboardTab;
        setTextData(&data, defaultTab, mimeCurrentTab);


#ifdef HAS_MOUSE_SELECTIONS
        if (mode == ClipboardMode::Clipboard ? m_storeClipboard : m_storeSelection) {
#else
        if (m_storeClipboard) {
#endif
            setTextData(&data, m_clipboardTab, mimeOutputTab);
        }

        emit clipboardChanged(data, ClipboardOwnership::Foreign);
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardmonitor.h"

#include "common/action.h"
#include "common/appconfig.h"
#include "common/common.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/serialize.h"
#include "platform/platformclipboard.h"

#ifdef COPYQ_WS_X11
#   include "platform/x11/x11info.h"
#endif

#include <QApplication>
#include <QClipboard>

namespace {

bool hasSameData(const QVariantMap &data, const QVariantMap &lastData)
{
    // Detect change also in case the data is unchanged but previously copied
    // by CopyQ and now externally. This solves storing a copied text which was
    // previously synchronized from selection to clipboard via CopyQ.
    if (
            !lastData.value(mimeOwner).toByteArray().isEmpty()
            && data.value(mimeOwner).toByteArray().isEmpty()
       )
    {
        return false;
    }

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

int defaultOwnerUpdateInterval()
{
#ifdef COPYQ_WS_X11
    if ( X11Info::isPlatformX11() )
        return 150;
#endif
    return 0;
}

} // namespace

ClipboardMonitor::ClipboardMonitor(const QStringList &formats)
    : m_clipboard(platformNativeInterface()->clipboard())
    , m_formats(formats)
    , m_ownerMonitor(this)
{
    const AppConfig config;
    m_storeClipboard = config.option<Config::check_clipboard>();
    m_clipboardTab = config.option<Config::clipboard_tab>();

    const int ownerUpdateInterval = config.option<Config::update_clipboard_owner_delay_ms>();
    m_ownerMonitor.setUpdateInterval(
        ownerUpdateInterval < 0 ? defaultOwnerUpdateInterval() : ownerUpdateInterval);

    m_formats.append({mimeOwner, mimeWindowTitle, mimeItemNotes, mimeHidden});
    m_formats.removeDuplicates();

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
#endif
}

void ClipboardMonitor::startMonitoring()
{
    setClipboardOwner(currentClipboardOwner());
    connect(QGuiApplication::clipboard(), &QClipboard::changed,
            this, [this](){ m_ownerMonitor.update(); });

    m_clipboard->startMonitoring(m_formats);
}

QString ClipboardMonitor::currentClipboardOwner()
{
    QString owner;
    emit fetchCurrentClipboardOwner(&owner);
    return owner;
}

void ClipboardMonitor::setClipboardOwner(const QString &owner)
{
    if (m_clipboardOwner != owner) {
        m_clipboardOwner = owner;
        m_clipboard->setClipboardOwner(m_clipboardOwner);
        COPYQ_LOG(QStringLiteral("Clipboard owner: %1").arg(owner));
    }
}

void ClipboardMonitor::onClipboardChanged(ClipboardMode mode)
{
    m_ownerMonitor.update();

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

    if ( !data.contains(mimeOwner)
        && !data.contains(mimeWindowTitle)
        && !m_clipboardOwner.isEmpty() )
    {
            data.insert(mimeWindowTitle, m_clipboardOwner.toUtf8());
    }

    COPYQ_LOG( QString("%1 changed, owner is \"%2\"")
               .arg(mode == ClipboardMode::Clipboard ? "Clipboard" : "Selection",
                    getTextData(data, mimeOwner)) );

#ifdef HAS_MOUSE_SELECTIONS
    if ( (mode == ClipboardMode::Clipboard ? m_clipboardToSelection : m_selectionToClipboard)
        && m_clipboard->isSelectionSupported()
        && !data.contains(mimeOwner) )
    {
        const auto text = getTextData(data);
        if ( !text.isEmpty() ) {
            const auto targetMode = mode == ClipboardMode::Clipboard
                ? ClipboardMode::Selection
                : ClipboardMode::Clipboard;
            const QVariantMap targetData = m_clipboard->data(targetMode, {mimeText});
            const uint targetTextHash = qHash( targetData.value(mimeText).toByteArray() );
            const uint sourceTextHash = qHash( data.value(mimeText).toByteArray() );
            emit synchronizeSelection(mode, sourceTextHash, targetTextHash);
        }
    }

    // omit running run automatic commands when disabled
    if ( !m_runSelection && mode == ClipboardMode::Selection ) {
        if ( m_storeSelection && !m_clipboardTab.isEmpty() ) {
            data.insert(mimeClipboardMode, QByteArrayLiteral("selection"));
            setTextData(&data, m_clipboardTab, mimeOutputTab);
            emit saveData(data);
        }
        return;
    }
#endif

    if (mode != ClipboardMode::Clipboard) {
        const QString modeName = mode == ClipboardMode::Selection
                ? QStringLiteral("selection")
                : QStringLiteral("find buffer");
        data.insert(mimeClipboardMode, modeName);
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

// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboardmonitor.h"

#include "common/appconfig.h"
#include "common/common.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/serialize.h"
#include "platform/platformclipboard.h"

#include <QApplication>
#include <QClipboard>
#include <QTimer>

#include <array>
#include <utility>

namespace {

constexpr std::array<int, 4> clipboardReadRetryDelayMs{50, 100, 200, 400};

int modeIndex(ClipboardMode mode)
{
    return mode == ClipboardMode::Clipboard ? 0 : 1;
}

bool hasSameData(const QVariantMap &data, const QVariantMap &lastData)
{
    for (auto it = lastData.constBegin(); it != lastData.constEnd(); ++it) {
        const auto &format = it.key();
        if ( !data.contains(format) )
            return false;
    }

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const auto &format = it.key();
        if ( !data[format].toByteArray().isEmpty()
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

bool isClipboardDataSecret(const QVariantMap &data)
{
    return data.value(mimeSecret).toByteArray() == "1";
}

} // namespace

ClipboardMonitor::ClipboardMonitor(
        const QStringList &formats, PlatformClipboardPtr clipboard)
    : m_clipboard(clipboard ? std::move(clipboard) : platformNativeInterface()->clipboard())
    , m_formats(formats)
    , m_ownerMonitor(this)
{
    const AppConfig config;
    m_storeClipboard = config.option<Config::check_clipboard>();
    m_clipboardTab = config.option<Config::clipboard_tab>();

    const int ownerUpdateInterval = config.option<Config::update_clipboard_owner_delay_ms>();
    m_ownerMonitor.setUpdateInterval(std::max(ownerUpdateInterval, 0));

    m_formats.append({mimeOwner, mimeWindowTitle, mimeItemNotes, mimeHidden, mimeSecret});
    m_formats.removeDuplicates();

    m_modes = ClipboardModeFlag::Clipboard;

#ifdef HAS_MOUSE_SELECTIONS
    m_storeSelection = config.option<Config::check_selection>();
    m_runSelection = config.option<Config::run_selection>();

    m_clipboardToSelection = config.option<Config::copy_clipboard>()
        && m_clipboard->isSelectionSupported();
    m_selectionToClipboard = config.option<Config::copy_selection>();

    if (!m_storeSelection && !m_runSelection && !m_selectionToClipboard) {
        COPYQ_LOG("Disabling selection monitoring");
    } else {
        m_modes |= ClipboardModeFlag::Selection;
    }
#endif
}

void ClipboardMonitor::startMonitoring()
{
    setClipboardOwner(currentClipboardOwner());
    connect(QGuiApplication::clipboard(), &QClipboard::changed,
            this, [this](){ m_ownerMonitor.update(); });

    m_connection = m_clipboard->createConnection(m_formats, m_modes);
    connect(m_connection.get(), &ClipboardConnection::changed,
            this, &ClipboardMonitor::onClipboardChanged);
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
    const int index = modeIndex(mode);
    const quint64 generation = ++m_readGenerations[index];
    processClipboardChanged(mode, generation, 0);
}

void ClipboardMonitor::processClipboardChanged(
        ClipboardMode mode, quint64 generation, int retryAttempt)
{
    m_ownerMonitor.update();

    const ClipboardReadResult readResult = m_clipboard->readData(mode, m_formats);
    if (!readResult.isComplete) {
        const int index = modeIndex(mode);
        if (generation != m_readGenerations[index])
            return;

        if (retryAttempt >= static_cast<int>(clipboardReadRetryDelayMs.size())) {
            log(QStringLiteral("Clipboard data remained unavailable after %1 retries; keeping the last valid snapshot")
                .arg(retryAttempt), LogWarning);
            return;
        }

        const int delay = clipboardReadRetryDelayMs[retryAttempt];
        COPYQ_LOG(QStringLiteral("Clipboard data unavailable; retrying in %1 ms")
                  .arg(delay));
        QTimer::singleShot(delay, this, [this, mode, generation, retryAttempt, index]() {
            if (generation == m_readGenerations[index])
                processClipboardChanged(mode, generation, retryAttempt + 1);
        });
        return;
    }

    QVariantMap data = readResult.data;
    auto clipboardData = mode == ClipboardMode::Clipboard
            ? &m_clipboardData : &m_selectionData;

    const bool isDataUnchanged = hasSameData(data, *clipboardData);
    if (!isDataUnchanged) {
        *clipboardData = data;
#ifdef HAS_MOUSE_SELECTIONS
    } else if (isDataUnchanged && !m_runSelection && mode == ClipboardMode::Selection) {
        return;
#endif
    }

    if ( !data.contains(mimeOwner)
        && !data.contains(mimeWindowTitle)
        && !m_clipboardOwner.isEmpty() )
    {
            data.insert(mimeWindowTitle, m_clipboardOwner.toUtf8());
    }

    COPYQ_LOG( QStringLiteral("%1 changed, owner is: %2")
               .arg(QLatin1String(mode == ClipboardMode::Clipboard ? "Clipboard" : "Selection"),
                    getTextData(data, mimeOwner)) );

    const auto defaultTab = m_clipboardTab.isEmpty() ? defaultClipboardTabName() : m_clipboardTab;
    setTextData(&data, defaultTab, mimeCurrentTab);

#ifdef HAS_MOUSE_SELECTIONS
    if (mode == ClipboardMode::Selection)
        data.insert(mimeClipboardMode, QByteArrayLiteral("selection"));

    if (mode == ClipboardMode::Clipboard ? m_storeClipboard : m_storeSelection) {
#else
    if (m_storeClipboard) {
#endif
        setTextData(&data, m_clipboardTab, mimeOutputTab);
    }

    if (isDataUnchanged) {
        emit clipboardUnchanged(data);
        return;
    }

#ifdef HAS_MOUSE_SELECTIONS
    if ( (mode == ClipboardMode::Clipboard ? m_clipboardToSelection : m_selectionToClipboard)
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
        if ( m_storeSelection && !m_clipboardTab.isEmpty() && !isClipboardDataSecret(data) )
            emit saveData(data);
        return;
    }
#endif

    // run script callbacks
    if ( isClipboardDataSecret(data) ) {
        emit secretClipboardChanged(data);
    } else if ( isClipboardDataHidden(data) ) {
        emit hiddenClipboardChanged(data);
    } else if ( anySessionOwnsClipboardData(data) ) {
        emit ownClipboardChanged(data);
    } else {
        emit clipboardChanged(data);
    }
}

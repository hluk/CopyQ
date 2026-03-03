// SPDX-License-Identifier: GPL-3.0-or-later

#include "platformclipboard.h"

#include <algorithm>

ClipboardModeFlag clipboardModeFlag(ClipboardMode mode)
{
    return mode == ClipboardMode::Clipboard
        ? ClipboardModeFlag::Clipboard
        : ClipboardModeFlag::Selection;
}

ClipboardConnection::ClipboardConnection(
        const QStringList &formats, ClipboardModeMask modes, PlatformClipboard *clipboard)
    : m_formats(formats)
    , m_modes(modes)
    , m_clipboard(clipboard)
{}

ClipboardConnection::~ClipboardConnection()
{
    if (m_clipboard)
        m_clipboard->unregisterConnection(this);
}

PlatformClipboard::~PlatformClipboard()
{
    for (auto *connection : std::as_const(m_connections))
        connection->m_clipboard = nullptr;
    m_connections.clear();
}

ClipboardConnectionPtr PlatformClipboard::createConnection(
        const QStringList &formats, ClipboardModeMask modes)
{
    auto connection = ClipboardConnectionPtr(new ClipboardConnection(formats, modes, this));
    registerConnection(connection.get());
    return connection;
}

void PlatformClipboard::registerConnection(ClipboardConnection *connection)
{
    m_connections.append(connection);
    setMonitoringState();
}

void PlatformClipboard::unregisterConnection(ClipboardConnection *connection)
{
    const auto it = std::remove(m_connections.begin(), m_connections.end(), connection);
    if (it == m_connections.end())
        return;

    m_connections.erase(it, m_connections.end());
    connection->m_clipboard = nullptr;
    setMonitoringState();
}

void PlatformClipboard::setMonitoringState()
{
    m_monitoredFormats.clear();
    m_monitoredModes = {};

    for (const auto *connection : std::as_const(m_connections)) {
        m_monitoredFormats.append(connection->formats());
        m_monitoredModes |= connection->modes();
    }
    m_monitoredFormats.removeDuplicates();

    if (m_connections.isEmpty()) {
        if (m_isMonitoringActive) {
            stopMonitoringBackend();
            m_isMonitoringActive = false;
        }
        return;
    }

    if (!m_isMonitoringActive) {
        startMonitoringBackend(m_monitoredFormats, m_monitoredModes);
        m_isMonitoringActive = true;
    } else {
        updateMonitoringSubscription(m_monitoredFormats, m_monitoredModes);
    }
}

void PlatformClipboard::updateMonitoringSubscription(const QStringList &, ClipboardModeMask)
{
}

void PlatformClipboard::emitConnectionChanged(ClipboardMode mode)
{
    const auto flag = clipboardModeFlag(mode);
    const auto connections = m_connections;
    for (auto *connection : connections) {
        if (connection->modes().testFlag(flag))
            emit connection->changed(mode);
    }
}

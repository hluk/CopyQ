/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once
#include <QClipboard>
#include <QElapsedTimer>
#include <QObject>
#include <memory>

class DataControlDevice;
class DataControlDeviceManager;
class KeyboardFocusWatcher;
class QMimeData;

class WaylandClipboard final : public QObject
{
    Q_OBJECT

public:
    static WaylandClipboard *instance();

    ~WaylandClipboard();

    void setMimeData(QMimeData *mime, QClipboard::Mode mode);
    const QMimeData *mimeData(QClipboard::Mode mode) const;
    bool waitForDevice(int timeoutMs = 5000) const;

signals:
    void changed(QClipboard::Mode mode);

private:
    explicit WaylandClipboard(QObject *parent);
    static WaylandClipboard *createInstance();
    void gainedFocus();
    std::shared_ptr<KeyboardFocusWatcher> m_keyboardFocusWatcher;

    std::unique_ptr<DataControlDeviceManager> m_manager;
    std::unique_ptr<DataControlDevice> m_device;
    QElapsedTimer m_deviceRequestedTimer;
};

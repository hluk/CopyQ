/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once
#include <QClipboard>
#include <QObject>
#include <memory>

class DataControlDevice;
class DataControlDeviceManager;
class QMimeData;

class WaylandClipboard final : public QObject
{
    Q_OBJECT

public:
    static WaylandClipboard *instance();

    ~WaylandClipboard();

    void setMimeData(QMimeData *mime, QClipboard::Mode mode);
    void clear(QClipboard::Mode mode);
    const QMimeData *mimeData(QClipboard::Mode mode) const;
    bool isActive() const { return m_device != nullptr; }
    bool isSelectionSupported() const;

signals:
    void changed(QClipboard::Mode mode);

private:
    explicit WaylandClipboard(QObject *parent);
    static WaylandClipboard *createInstance();

    std::unique_ptr<DataControlDeviceManager> m_manager;
    std::unique_ptr<DataControlDevice> m_device;
};

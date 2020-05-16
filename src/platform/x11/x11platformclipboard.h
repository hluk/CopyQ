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

#ifndef X11PLATFORMCLIPBOARD_H
#define X11PLATFORMCLIPBOARD_H

#include "platform/dummy/dummyclipboard.h"

#include <QByteArray>
#include <QStringList>
#include <QTimer>

#include <memory>

class X11PlatformClipboard final : public DummyClipboard
{
public:
    X11PlatformClipboard();

    void startMonitoring(const QStringList &formats) override;

    void setMonitoringEnabled(ClipboardMode mode, bool enable) override;

    QVariantMap data(ClipboardMode mode, const QStringList &formats) const override;

    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;

protected:
    void onChanged(int mode) override;

private:
    struct ClipboardData {
        QVariantMap newData;
        QVariantMap data;
        QByteArray owner;
        QByteArray newOwner;
        QTimer timerEmitChange;
        QStringList formats;
        QByteArray newDataTimestamp;
        ClipboardMode mode;
        bool enabled = true;
        bool cloningData = false;
        bool abortCloning = false;
        int retry = 0;
    };

    void check();
    void updateClipboardData(ClipboardData *clipboardData);
    void useNewClipboardData(ClipboardData *clipboardData);
    void checkAgainLater(bool clipboardChanged, int interval);

    QTimer m_timerCheckAgain;

    ClipboardData m_clipboardData;
    ClipboardData m_selectionData;
};

#endif // X11PLATFORMCLIPBOARD_H

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

#ifndef DUMMYCLIPBOARD_H
#define DUMMYCLIPBOARD_H

#include "app/clipboardownermonitor.h"
#include "common/clipboardmode.h"
#include "platform/platformclipboard.h"

#include <QClipboard>

class DummyClipboard : public PlatformClipboard
{
public:
    void startMonitoring(const QStringList &) override;

    void setMonitoringEnabled(ClipboardMode, bool) override {}

    QVariantMap data(ClipboardMode mode, const QStringList &formats) const override;

    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;

    QByteArray clipboardOwner() override { return m_ownerMonitor.clipboardOwner(); }

protected:
    virtual void onChanged(int mode);

private:
    void onClipboardChanged(QClipboard::Mode mode);

    ClipboardOwnerMonitor m_ownerMonitor;
};

#endif // DUMMYCLIPBOARD_H

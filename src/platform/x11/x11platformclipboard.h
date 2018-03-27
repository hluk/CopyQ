/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include <QStringList>
#include <QTimer>

#include <memory>

class X11DisplayGuard;

class X11PlatformClipboard : public DummyClipboard
{
    Q_OBJECT
public:
    explicit X11PlatformClipboard(const std::shared_ptr<X11DisplayGuard> &d);

    void setFormats(const QStringList &formats) override;

    QVariantMap data(ClipboardMode mode, const QStringList &formats) const override;

    void setData(ClipboardMode mode, const QVariantMap &dataMap) override;

protected:
    void onChanged(int mode) override;

private slots:
    void onClipboardChanged();
    void onSelectionChanged();

private:
    bool waitIfSelectionIncomplete();

    std::shared_ptr<X11DisplayGuard> d;

    QStringList m_formats;

    QTimer m_timerCheckClipboard;
    QTimer m_timerCheckSelection;

    QVariantMap m_clipboardData;
    QVariantMap m_selectionData;
};

#endif // X11PLATFORMCLIPBOARD_H

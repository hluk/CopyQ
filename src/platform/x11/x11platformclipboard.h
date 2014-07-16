/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include <QSharedPointer>

class X11DisplayGuard;

class X11PlatformClipboard : public DummyClipboard
{
    Q_OBJECT
public:
    X11PlatformClipboard(const QSharedPointer<X11DisplayGuard> &d);

    void loadSettings(const QVariantMap &settings);

    QVariantMap data(const QStringList &formats) const;

private slots:
    void onChanged(QClipboard::Mode mode);

private:
    QSharedPointer<X11DisplayGuard> d;

    bool m_copyclip;
    bool m_checksel;
    bool m_copysel;

    bool m_lastChangedIsClipboard;
};

#endif // X11PLATFORMCLIPBOARD_H

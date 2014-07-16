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

#include <QApplication>
#include <QClipboard>

#include "x11platformclipboard.h"

#include "x11displayguard.h"

#include "common/common.h"

X11PlatformClipboard::X11PlatformClipboard(const QSharedPointer<X11DisplayGuard> &d)
    : d(d)
    , m_copyclip(false)
    , m_checksel(false)
    , m_copysel(false)
    , m_lastChangedIsClipboard(true)
{
}

void X11PlatformClipboard::loadSettings(const QVariantMap &settings)
{
    if ( settings.contains("copy_clipboard") )
        m_copyclip = settings["copy_clipboard"].toBool();
    if ( settings.contains("copy_selection") )
        m_copysel = settings["copy_selection"].toBool();
    if ( settings.contains("check_selection") )
        m_checksel = settings["check_selection"].toBool();
}

QVariantMap X11PlatformClipboard::data(const QStringList &formats) const
{
    return DummyClipboard::data(
                m_lastChangedIsClipboard ? QClipboard::Clipboard : QClipboard::Selection, formats);
}

void X11PlatformClipboard::onChanged(QClipboard::Mode mode)
{
    m_lastChangedIsClipboard = mode == QClipboard::Clipboard;
    emit changed();
}

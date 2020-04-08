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

#include "clipboardspy.h"

#include "common/common.h"

#include <QApplication>
#include <QClipboard>
#include <QTimer>

namespace {

template <typename Receiver, typename Slot>
void connectClipboardSignal(ClipboardMode mode, Receiver *receiver, Slot slot)
{
    const auto signal = mode == ClipboardMode::Clipboard
            ? &QClipboard::dataChanged
            : &QClipboard::selectionChanged;
    QObject::connect( QApplication::clipboard(), signal, receiver, slot );
}

} // namespace

ClipboardSpy::ClipboardSpy(ClipboardMode mode)
    : m_mode(mode)
    , m_oldOwnerData(clipboardOwnerData(mode))
{
    connectClipboardSignal(mode, this, &ClipboardSpy::onChanged);
}

void ClipboardSpy::wait(int ms)
{
    if (m_changed || check())
        return;

    QEventLoop loop;
    connectClipboardSignal(m_mode, &loop, &QEventLoop::quit);

    connect( this, &ClipboardSpy::changed, &loop, &QEventLoop::quit );

    QTimer timerStop;
    timerStop.setInterval(ms);
    connect( &timerStop, &QTimer::timeout, &loop, &QEventLoop::quit );
    timerStop.start();

    QTimer timerCheck;
    timerCheck.setInterval(100);
    connect( &timerCheck, &QTimer::timeout, this, &ClipboardSpy::check );
    timerCheck.start();

    loop.exec();
}

void ClipboardSpy::onChanged()
{
    m_changed = true;
    emit changed();
}

bool ClipboardSpy::check()
{
    if (m_oldOwnerData == clipboardOwnerData(m_mode))
        return false;

    onChanged();
    return true;
}

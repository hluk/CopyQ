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

#include "clipboardspy.h"

#include <QApplication>
#include <QClipboard>
#include <QTimer>

namespace {

void connectClipboardSignal(ClipboardMode mode, QObject *receiver, const char *slot)
{
    const auto signal = mode == ClipboardMode::Clipboard
            ? SIGNAL(dataChanged())
            : SIGNAL(selectionChanged());
    QObject::connect( QApplication::clipboard(), signal, receiver, slot );
}

} // namespace

ClipboardSpy::ClipboardSpy(ClipboardMode mode)
    : m_mode(mode)
{
    connectClipboardSignal(mode, this, SLOT(onChanged()));
}

void ClipboardSpy::wait(int ms)
{
    if (m_changed)
        return;

    QEventLoop loop;
    connectClipboardSignal(m_mode, &loop, SLOT(quit()));

    QTimer t;
    t.setInterval(ms);
    connect( &t, SIGNAL(timeout()), &loop, SLOT(quit()) );

    t.start();
    loop.exec();
}

void ClipboardSpy::onChanged()
{
    m_changed = true;
}

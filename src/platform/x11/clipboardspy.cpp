/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#include <QElapsedTimer>

ClipboardSpy::ClipboardSpy()
    : m_clipboardChanged(false)
{
    connect( QApplication::clipboard(), SIGNAL(dataChanged()),
             this, SLOT(clipboardChanged()) );
}

void ClipboardSpy::wait(int ms)
{
    QElapsedTimer t;
    t.start();
    while ( !m_clipboardChanged && t.elapsed() < ms )
        QApplication::processEvents(QEventLoop::AllEvents, 100);
}

void ClipboardSpy::clipboardChanged()
{
    m_clipboardChanged = true;
}

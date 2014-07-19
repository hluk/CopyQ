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

#include "dummyclipboard.h"

#include "common/common.h"

#include <QApplication>

DummyClipboard::DummyClipboard(bool connectClipboardSignal)
{
    if (connectClipboardSignal) {
        connect( QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)),
                 this, SLOT(onChanged(QClipboard::Mode)) );
    }
}

QVariantMap DummyClipboard::data(const QStringList &formats) const
{
    return data(QClipboard::Clipboard, formats);
}

void DummyClipboard::setData(const QVariantMap &dataMap)
{
    setData(QClipboard::Clipboard, dataMap);
}

QVariantMap DummyClipboard::data(QClipboard::Mode mode, const QStringList &formats) const
{
    const QMimeData *data = clipboardData(mode);
    return data ? cloneData(*data, formats) : QVariantMap();
}

void DummyClipboard::setData(QClipboard::Mode mode, const QVariantMap &dataMap)
{
    Q_ASSERT( isMainThread() );

    QApplication::clipboard()->setMimeData( createMimeData(dataMap), mode );
}

void DummyClipboard::onChanged(QClipboard::Mode mode)
{
    if (mode == QClipboard::Clipboard)
        emit changed();
}

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

#include "dummyclipboard.h"

#include "common/common.h"
#include "common/log.h"

#include <QGuiApplication>
#include <QStringList>

QClipboard::Mode modeToQClipboardMode(ClipboardMode mode)
{
    switch (mode) {
    case ClipboardMode::Selection:
        return QClipboard::Selection;
    case ClipboardMode::Clipboard:
        return QClipboard::Clipboard;
    }

    Q_ASSERT(false);
    return QClipboard::Clipboard;
}

void DummyClipboard::startMonitoring(const QStringList &)
{
    connect(QGuiApplication::clipboard(), &QClipboard::changed,
            this, &DummyClipboard::onClipboardChanged);
}

QVariantMap DummyClipboard::data(ClipboardMode mode, const QStringList &formats) const
{
    const QMimeData *data = mimeData(mode);
    return data ? cloneData(*data, formats) : QVariantMap();
}

void DummyClipboard::setData(ClipboardMode mode, const QVariantMap &dataMap)
{
    Q_ASSERT( isMainThread() );

    QGuiApplication::clipboard()->setMimeData( createMimeData(dataMap), modeToQClipboardMode(mode) );
}

const QMimeData *DummyClipboard::mimeData(ClipboardMode mode) const
{
    const auto modeText = mode == ClipboardMode::Clipboard ? "clipboard" : "selection";

    COPYQ_LOG_VERBOSE( QString("Getting %1 data.").arg(modeText) );
    const QMimeData *data = QGuiApplication::clipboard()->mimeData( modeToQClipboardMode(mode) );

    if (data)
        COPYQ_LOG_VERBOSE( QString("Got %1 data.").arg(modeText) );
    else
        log( QString("Null data in %1.").arg(modeText), LogError );

    return data;
}

void DummyClipboard::onChanged(int mode)
{
    if (mode == QClipboard::Clipboard)
        emit changed(ClipboardMode::Clipboard);
}

void DummyClipboard::onClipboardChanged(QClipboard::Mode mode)
{
    onChanged(static_cast<int>(mode));
}

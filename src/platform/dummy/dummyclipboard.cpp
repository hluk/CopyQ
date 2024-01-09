// SPDX-License-Identifier: GPL-3.0-or-later

#include "dummyclipboard.h"

#include "common/common.h"
#include "common/log.h"
#include "common/mimetypes.h"

#include <QGuiApplication>
#include <QMimeData>
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
    QGuiApplication::clipboard()->setMimeData( createMimeData(dataMap), modeToQClipboardMode(mode) );
}

const QMimeData *DummyClipboard::rawMimeData(ClipboardMode mode) const
{
    return QGuiApplication::clipboard()->mimeData( modeToQClipboardMode(mode) );
}

const QMimeData *DummyClipboard::mimeData(ClipboardMode mode) const
{
    const auto modeText = mode == ClipboardMode::Clipboard ? "clipboard" : "selection";

    COPYQ_LOG_VERBOSE( QStringLiteral("Getting %1 data").arg(modeText) );
    const QMimeData *data = rawMimeData(mode);

    if (!data) {
        log( QStringLiteral("Null data in %1").arg(modeText), LogError );
        return nullptr;
    }

    if (isHidden(*data)) {
        log( QStringLiteral("Hiding secret %1 data").arg(modeText) );
        return nullptr;
    }

    COPYQ_LOG_VERBOSE( QStringLiteral("Got %1 data").arg(modeText) );
    return data;
}

bool DummyClipboard::isHidden(const QMimeData &data) const
{
    const QByteArray passwordManagerHint = data.data(QStringLiteral("x-kde-passwordManagerHint"));
    return passwordManagerHint == QByteArrayLiteral("secret");
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

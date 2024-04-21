// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboarditem.h"

#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/serialize.h"

#include <QBrush>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace {

void clearDataExceptInternal(QVariantMap *data)
{
    QMutableMapIterator<QString, QVariant> it(*data);
    while (it.hasNext()) {
        const auto item = it.next();
        if ( !item.key().startsWith(COPYQ_MIME_PREFIX) )
            it.remove();
    }
}

} // namespace

ClipboardItem::ClipboardItem()
    : m_data()
    , m_hash(0)
{
}

ClipboardItem::ClipboardItem(const QVariantMap &data)
    : m_data(data)
    , m_hash(0)
{
}

bool ClipboardItem::operator ==(const ClipboardItem &item) const
{
    return dataHash() == item.dataHash();
}

void ClipboardItem::setText(const QString &text)
{
    QMutableMapIterator<QString, QVariant> it(m_data);
    while (it.hasNext()) {
        const auto item = it.next();
        if ( item.key().startsWith("text/") )
            it.remove();
    }

    setTextData(&m_data, text);

    invalidateDataHash();
}

bool ClipboardItem::setData(const QVariantMap &data)
{
    if (m_data == data)
        return false;

    const auto oldData = m_data;
    m_data = data;

    for (auto it = oldData.constBegin(); it != oldData.constEnd(); ++it) {
        if (it.key().startsWith(mimePrivatePrefix) && !m_data.contains(it.key()))
            m_data[it.key()] = it.value();
    }

    invalidateDataHash();
    return true;
}

bool ClipboardItem::updateData(const QVariantMap &data)
{
    const int oldSize = m_data.size();
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const auto &format = it.key();
        if ( !format.startsWith(COPYQ_MIME_PREFIX) ) {
            clearDataExceptInternal(&m_data);
            break;
        }
    }

    bool changed = (oldSize != m_data.size());

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const auto &format = it.key();
        const auto &value = it.value();
        if ( !value.isValid() ) {
            m_data.remove(format);
            changed = true;
        } else if ( m_data.value(format) != value ) {
            m_data.insert(format, value);
            changed = true;
        }
    }

    invalidateDataHash();

    return changed;
}

void ClipboardItem::removeData(const QString &mimeType)
{
    m_data.remove(mimeType);
    invalidateDataHash();
}

bool ClipboardItem::removeData(const QStringList &mimeTypeList)
{
    bool removed = false;

    for (const auto &mimeType : mimeTypeList) {
        if ( m_data.contains(mimeType) ) {
            m_data.remove(mimeType);
            removed = true;
        }
    }

    if (removed)
        invalidateDataHash();

    return removed;
}

void ClipboardItem::setData(const QString &mimeType, const QByteArray &data)
{
    m_data.insert(mimeType, data);
    invalidateDataHash();
}

QVariant ClipboardItem::data(int role) const
{
    switch(role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return getTextData(m_data);

    case contentType::data:
        return m_data; // copy-on-write, so this should be fast
    case contentType::hasText:
        return m_data.contains(mimeText)
            || m_data.contains(mimeTextUtf8)
            || m_data.contains(mimeUriList);
    case contentType::hasHtml:
        return m_data.contains(mimeHtml);
    case contentType::text:
        return getTextData(m_data);
    case contentType::html:
        return getTextData(m_data, mimeHtml);
    case contentType::notes:
        return getTextData(m_data, mimeItemNotes);
    case contentType::color:
        return getTextData(m_data, mimeColor);
    case contentType::isHidden:
        return m_data.contains(mimeHidden);
    }

    return QVariant();
}

unsigned int ClipboardItem::dataHash() const
{
    if (m_hash == 0)
        m_hash = hash(m_data);

    return m_hash;
}

void ClipboardItem::invalidateDataHash()
{
    m_hash = 0;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

#include <QUtiMimeConverter>
using UrlPasteboardMimeBase = QUtiMimeConverter;

/**
 * Class for doing conversions of URLs between OS X UTIs and "normal" mimeTypes.
 */
class UrlPasteboardMime final : public UrlPasteboardMimeBase {
public:
    //! Create an instance of UrlPasteboardMime which will try to convert to/from the given UTI.
    explicit UrlPasteboardMime(const QString &urlUti);

    QString mimeForUti(const QString &uti) const override;
    QString utiForMime(const QString &mime) const override;
    QVariant convertToMime(const QString &mime, const QList<QByteArray> &data, const QString &uti) const override;
    QList<QByteArray> convertFromMime(const QString &mime, const QVariant &data, const QString &uti) const override;
private:
    QString m_urlUti;
};

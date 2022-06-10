// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#   include <QUtiMimeConverter>
using UrlPasteboardMimeBase = QUtiMimeConverter;
#else
#   include <QMacPasteboardMime>
using UrlPasteboardMimeBase = QMacPasteboardMime;
#endif

/**
 * Class for doing conversions of URLs between OS X UTIs and "normal" mimeTypes.
 */
class UrlPasteboardMime final : public UrlPasteboardMimeBase {
public:
    //! Create an instance of UrlPasteboardMime which will try to convert to/from the given UTI.
    explicit UrlPasteboardMime(const QString &urlUti);

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QString mimeForUti(const QString &uti) const override;
    QString utiForMime(const QString &mime) const override;
    QVariant convertToMime(const QString &mime, const QList<QByteArray> &data, const QString &uti) const override;
    QList<QByteArray> convertFromMime(const QString &mime, const QVariant &data, const QString &uti) const override;
#else
    QString convertorName() override;
    QString flavorFor(const QString &mime) override;
    QString mimeFor(QString uti) override;
    bool canConvert(const QString &mime, QString uti) override;
    QVariant convertToMime(const QString &mime, QList<QByteArray> data, QString uti) override;
    QList<QByteArray> convertFromMime(const QString &mime, QVariant data, QString uti) override;
#endif
private:
    QString m_urlUti;
};

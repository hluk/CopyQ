// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef URLPASTEBOARDMIME_H
#define URLPASTEBOARDMIME_H

#include <QMacPasteboardMime>

/**
 * Class for doing conversions of URLs between OS X UTIs and "normal" mimeTypes.
 */
class UrlPasteboardMime final : public QMacPasteboardMime {
public:
    //! Create an instance of UrlPasteboardMime which will try to convert to/from the given UTI.
    UrlPasteboardMime(const QString &urlUti);
    QString convertorName();
    QString flavorFor(const QString &mime);
    QString mimeFor(QString flav);
    bool canConvert(const QString &mime, QString flav);
    QVariant convertToMime(const QString &mime, QList<QByteArray> data, QString flav);
    QList<QByteArray> convertFromMime(const QString &mime, QVariant data, QString flav);
private:
    QString m_urlUti;
};

#endif // URLPASTEBOARDMIME_H

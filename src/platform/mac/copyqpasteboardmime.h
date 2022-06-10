// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#   include <QUtiMimeConverter>
using CopyqPasteboardMimeBase = QUtiMimeConverter;
#else
#   include <QMacPasteboardMime>
using CopyqPasteboardMimeBase = QMacPasteboardMime;
#endif

/**
 * Class for doing lossless conversions between OS X UTIs and "normal" mimeTypes.
 *
 * Useful links about UTIs:
 * - https://developer.apple.com/library/mac/documentation/FileManagement/Conceptual/understanding_utis/understand_utis_intro/understand_utis_intro.html
 * - https://developer.apple.com/library/mac/documentation/MobileCoreServices/Reference/UTTypeRef/Reference/reference.html
 * - http://alastairs-place.net/blog/2012/06/06/utis-are-better-than-you-think-and-heres-why/
 *
 * Useful links about QMacPasteboardMime (and it's usage):
 * - http://doc-snapshot.qt-project.org/qdoc/qmacpasteboardmime.html
 * - https://www.qt.gitorious.org/qt/qt/source/src/gui/kernel/qmime_mac.cpp
 * - https://www.qt.gitorious.org/qt/qt/source/src/gui/kernel/qclipboard_mac.cpp
 */

class CopyQPasteboardMime final : public CopyqPasteboardMimeBase {
public:
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QString mimeForUti(const QString &uti) const override;
    QString utiForMime(const QString &mime) const override;
    QVariant convertToMime(const QString &mime, const QList<QByteArray> &data, const QString &uti) const override;
    QList<QByteArray> convertFromMime(const QString &mime, const QVariant &data, const QString &uti) const override;
#else
    CopyQPasteboardMime() : CopyqPasteboardMimeBase(MIME_ALL) {}
    QString convertorName() override;
    QString flavorFor(const QString &mime) override;
    QString mimeFor(QString uti) override;
    bool canConvert(const QString &mime, QString uti) override;
    QVariant convertToMime(const QString &mime, QList<QByteArray> data, QString uti) override;
    QList<QByteArray> convertFromMime(const QString &mime, QVariant data, QString uti) override;
#endif
};

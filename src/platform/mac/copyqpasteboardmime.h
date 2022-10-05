// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MACMIME_H
#define MACMIME_H

#include <QMacPasteboardMime>

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

class CopyQPasteboardMime final : public QMacPasteboardMime {
public:
    CopyQPasteboardMime() : QMacPasteboardMime(MIME_ALL) { }
    QString convertorName() override;

    QString flavorFor(const QString &mime) override;
    QString mimeFor(QString flav) override;
    bool canConvert(const QString &mime, QString flav) override;
    QVariant convertToMime(const QString &mime, QList<QByteArray> data, QString flav) override;
    QList<QByteArray> convertFromMime(const QString &mime, QVariant data, QString flav) override;
};

#endif // MACMIME_H

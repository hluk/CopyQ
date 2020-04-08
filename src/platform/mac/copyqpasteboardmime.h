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

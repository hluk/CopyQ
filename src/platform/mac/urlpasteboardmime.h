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

/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "macmime.h"

#include <common/common.h>

#include <QClipboard>
#include <QEvent>

#import <CoreServices/CoreServices.h>

namespace {
    // MIME prefix used to convert OS X UTIs without associated mime types.
    const QString COPYQ_MIME_PREFIX("application/x-copyq-uti-");

    // types handled by other (builtin) converters
    const QStringList IGNORED_UTIS = QStringList()
        << "public.html"
        << "public.url"
        << "public.file-url"
        << "public.utf8-plain-text"
        << "public.utf16-plain-text"
        << "public.utf16-external-plain-text"
        << "com.apple.traditional-mac-plain-text";

    // types handled by other (builtin) converters
    const QStringList IGNORED_MIMES = QStringList()
        << "application/x-qt-image"
        << "text/html"
        << "text/plain"
        << "text/uri-list"
        << "application/x-copyq-owner-window-title";


    QString mimeToUTI(const QString &mime) {
        NSString *mimeString = mime.toNSString();
        CFStringRef MIMEType = (__bridge CFStringRef)(mimeString);
        CFStringRef UTI = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, MIMEType, NULL);
        NSString *UTIString = (__bridge NSString *)UTI;
        QString uti = QString::fromNSString(UTIString);
        if (UTI)
            CFRelease(UTI);
        return uti;
    }

    QString utiToMime(const QString &uti) {
        NSString *UTIString = uti.toNSString();
        CFStringRef UTI = (__bridge CFStringRef)(UTIString);
        CFStringRef MIMEType = UTTypeCopyPreferredTagWithClass(UTI, kUTTagClassMIMEType);
        NSString *MIMETypeString = (__bridge NSString *)MIMEType;
        QString mime = QString::fromNSString(MIMETypeString);
        if (MIMEType)
            CFRelease(MIMEType);
        return mime;
    }
}

QString CopyQPasteboardMime::convertorName()
{
    return QLatin1String("CopyQ");
}

QString CopyQPasteboardMime::flavorFor(const QString &mime)
{
    if (IGNORED_MIMES.contains(mime)) {
        return QString();
    }

    QString uti = mimeToUTI(mime);
    if (!uti.isEmpty() && !uti.startsWith("dyn.")) {
        return uti;
    } else if (mime.startsWith(COPYQ_MIME_PREFIX)) {
        return mime.mid(COPYQ_MIME_PREFIX.length());
    }
    return QString();
}

QString CopyQPasteboardMime::mimeFor(QString uti)
{
    if (IGNORED_UTIS.contains(uti)) {
        return QString();
    }

    QString mime = utiToMime(uti);
    if (!mime.isEmpty()) {
        return mime;
    }

    return COPYQ_MIME_PREFIX + uti;
}

bool CopyQPasteboardMime::canConvert(const QString &mime, QString uti)
{
    if (uti.isEmpty() || mime.isEmpty()) {
        return false;
    } else if (IGNORED_MIMES.contains(mime)) {
        return false;
    } else if (IGNORED_UTIS.contains(uti)) {
        return false;
    }

    QString convMime = utiToMime(uti);

#ifdef COPYQ_LOG_DEBUG
    if (convMime != mime || convMime.isEmpty()) {
        COPYQ_LOG(QString("Can't convert between %1 and %2").arg(uti).arg(mime));
    }
#endif

    return (convMime == mime);
}


QVariant CopyQPasteboardMime::convertToMime(const QString &mime, QList<QByteArray> data, QString uti)
{
    if (!canConvert(mime, uti)) {
        return QVariant();
    }

    if (data.count() == 1) {
        return QVariant::fromValue(data[0]);
    } else {
        QVariantList ret;
        foreach(const QByteArray &item, data) {
            ret << QVariant::fromValue(item);
        }
        return ret;
    }
}

QList<QByteArray> CopyQPasteboardMime::convertFromMime(const QString &mime, QVariant variant,
                                                          QString uti)
{
    if (!canConvert(mime, uti)) {
        return QList<QByteArray>();
    }

    QList<QByteArray> ret;
    if (variant.userType() == QMetaType::QByteArray) {
        // Was a single item
        ret << variant.toByteArray();
    } else {
        // Was a list or null
        QVariantList inp = variant.toList();
        foreach(const QVariant &item, inp) {
            ret << item.toByteArray();
        }
    }
    return ret;
}

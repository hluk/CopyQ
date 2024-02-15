// SPDX-License-Identifier: GPL-3.0-or-later

#include "copyqpasteboardmime.h"

#include "common/log.h"
#include "common/mimetypes.h"

#include <QClipboard>
#include <QEvent>
#include <QVariant>

#import <CoreServices/CoreServices.h>

namespace {
    // MIME prefix used by other Qt apps to handle unknown mime types
    const QString QT_UTI_PREFIX("com.trolltech.anymime.");

    // types handled by other (builtin) converters
    bool shouldIgnoreUTI(const QString &uti) {
        return uti == QLatin1String("public.html")
            || uti == QLatin1String("public.url")
            || uti == QLatin1String("public.file-url")
            || uti == QLatin1String("public.utf8-plain-text")
            || uti == QLatin1String("public.utf16-plain-text")
            || uti == QLatin1String("public.utf16-external-plain-text")
            || uti == QLatin1String("com.apple.traditional-mac-plain-text");
    }

    // types handled by other (builtin) converters
    bool shouldIgnoreMime(const QString &mime) {
        return mime == QLatin1String("application/x-qt-image")
            || mime == QLatin1String("text/html")
            || mime == QLatin1String("text/plain")
            || mime == QLatin1String("text/uri-list")
            || mime == QLatin1String("application/x-copyq-owner-window-title");
    }

    CFStringRef mimeToUTI(CFStringRef mime) {
        return UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mime, NULL);
    }

    CFStringRef utiToMime(CFStringRef uti) {
        return UTTypeCopyPreferredTagWithClass((__bridge CFStringRef)uti, kUTTagClassMIMEType);
    }

    QString convertUtiOrMime(const QString &in, CFStringRef (*convert)(CFStringRef)) {
        NSString *inString = in.toNSString();
        CFStringRef outRef = convert((__bridge CFStringRef)inString);
        QString out;
        if (outRef) {
            out = QString::fromNSString((__bridge NSString *)outRef);
            CFRelease(outRef);
        }
        return out;
    }

}

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
QString CopyQPasteboardMime::convertorName()
{
    return QLatin1String("CopyQ");
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
QString CopyQPasteboardMime::utiForMime(const QString &mime) const
#else
QString CopyQPasteboardMime::flavorFor(const QString &mime)
#endif
{
    if (shouldIgnoreMime(mime)) {
        return QString();
    }

    QString uti = convertUtiOrMime(mime, mimeToUTI);
    if (!uti.isEmpty()) {
        return uti;
    }

    if (mime.startsWith(COPYQ_MIME_PREFIX)) {
        return mime.mid(QLatin1String(COPYQ_MIME_PREFIX).size());
    }

    return QString();
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
QString CopyQPasteboardMime::mimeForUti(const QString &uti) const
#else
QString CopyQPasteboardMime::mimeFor(QString uti)
#endif
{
    if (shouldIgnoreUTI(uti)) {
        return QString();
    }

    // Handle mime data that Qt has converted by force to UTI
    if (uti.startsWith(QT_UTI_PREFIX)) {
        return uti.mid(QT_UTI_PREFIX.length()).replace("--", "/");
    }

    QString mime = convertUtiOrMime(uti, utiToMime);
    if (!mime.isEmpty()) {
        return mime;
    }

    return COPYQ_MIME_PREFIX + uti;
}

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
bool CopyQPasteboardMime::canConvert(const QString &mime, QString uti)
{
    if (uti.isEmpty() || mime.isEmpty())
        return false;

    // Allow to set UTI directly in QMimeData.
    if (mime == uti)
        return true;

    if (shouldIgnoreMime(mime))
        return false;

    if (shouldIgnoreUTI(uti))
        return false;

    QString convMime = convertUtiOrMime(uti, utiToMime);

    if (convMime != mime) {
        COPYQ_LOG(QString("Can't convert between %1 and %2").arg(uti).arg(mime));
    }

    return (convMime == mime);
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
QVariant CopyQPasteboardMime::convertToMime(const QString &mime, const QList<QByteArray> &data, const QString &uti) const
#else
QVariant CopyQPasteboardMime::convertToMime(const QString &mime, QList<QByteArray> data, QString uti)
#endif
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

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
QList<QByteArray> CopyQPasteboardMime::convertFromMime(const QString &mime, const QVariant &data, const QString &uti) const
#else
QList<QByteArray> CopyQPasteboardMime::convertFromMime(const QString &mime, QVariant data, QString uti)
#endif
{
    if (!canConvert(mime, uti)) {
        return QList<QByteArray>();
    }

    QList<QByteArray> ret;
    if (data.userType() == QMetaType::QByteArray) {
        // Was a single item
        ret << data.toByteArray();
    } else {
        // Was a list or null
        QVariantList inp = data.toList();
        foreach(const QVariant &item, inp) {
            ret << item.toByteArray();
        }
    }
    return ret;
}

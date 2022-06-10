// SPDX-License-Identifier: GPL-3.0-or-later

#include "urlpasteboardmime.h"

#include "common/mimetypes.h"

#import <CoreServices/CoreServices.h>
#import <Cocoa/Cocoa.h>

#import <QUrl>
#import <QVariant>

/*
 * Much of this code is based on the Qt code here:
 *  https://www.qt.gitorious.org/qt/qt/source/src/gui/kernel/qmime_mac.cpp#L765
 */

UrlPasteboardMime::UrlPasteboardMime(const QString &urlUti)
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    : UrlPasteboardMimeBase()
#else
    : UrlPasteboardMimeBase(MIME_ALL)
#endif
    , m_urlUti(urlUti)
{
}

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
QString UrlPasteboardMime::convertorName()
{
    return QLatin1String("CopyQ-URL-") + m_urlUti;
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
QString UrlPasteboardMime::utiForMime(const QString &mime) const
#else
QString UrlPasteboardMime::flavorFor(const QString &mime)
#endif
{
    if (mime.startsWith(QLatin1String(mimeUriList))) {
        return m_urlUti;
    }
    return QString();
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
QString UrlPasteboardMime::mimeForUti(const QString &uti) const
#else
QString UrlPasteboardMime::mimeFor(QString uti)
#endif
{
    return (uti == m_urlUti) ? mimeUriList : QString();
}

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
bool UrlPasteboardMime::canConvert(const QString &mime, QString uti)
{
    return mime == QLatin1String(mimeUriList) && uti == m_urlUti;
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
QVariant UrlPasteboardMime::convertToMime(const QString &mime, const QList<QByteArray> &data, const QString &uti) const
#else
QVariant UrlPasteboardMime::convertToMime(const QString &mime, QList<QByteArray> data, QString uti)
#endif
{
    if (!canConvert(mime, uti))
        return QVariant();

    QList<QVariant> ret;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    foreach (const QByteArray &urlBytes, data) {
        QUrl url = QUrl::fromEncoded(urlBytes);
        if (url.scheme().toLower() == QLatin1String("file")) {
            // If this is a "file://" URL, we probably got a fileReference URL, which is less
            // understandable by the user, so we'll change it to a filePath URL instead.
            NSURL *nsUrl = [NSURL URLWithString: url.toString(QUrl::FullyEncoded).toNSString()];
            url = QUrl::fromNSURL([nsUrl filePathURL]);
        }
        if (url.host().toLower() == QLatin1String("localhost"))
            url.setHost(QString());
        url.setPath(url.path().normalized(QString::NormalizationForm_C));
        ret.append(url);
    }
    [pool release];

    return QVariant(ret);
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
QList<QByteArray> UrlPasteboardMime::convertFromMime(const QString &mime, const QVariant &data, const QString &uti) const
#else
QList<QByteArray> UrlPasteboardMime::convertFromMime(const QString &mime, QVariant data, QString uti)
#endif
{
    QList<QByteArray> ret;
    if (!canConvert(mime, uti))
        return ret;

    QVariantList dataList = data.toList();
    if (dataList.isEmpty()) {
        // Data has been converted to a newline separated byte array (??)
        QStringList urls = data.toString().split('\n');
        foreach (const QUrl &url, QUrl::fromStringList(urls)) {
            dataList << url;
        }
    }

    foreach (const QVariant &urlVariant, dataList) {
        QUrl url = urlVariant.toUrl();
        if (url.scheme().isEmpty())
            url.setScheme(QLatin1String("file"));
        if (url.scheme().toLower() == QLatin1String("file")) {
            // If this is a "file://" URL, OS X probably wants a fileReference URL
            NSURL *nsUrl = [NSURL URLWithString: url.toString(QUrl::FullyEncoded).toNSString()];
            url = QUrl::fromNSURL([nsUrl fileReferenceURL]);
            url.setPath(url.path().normalized(QString::NormalizationForm_D));
        }
        ret.append(url.toEncoded());
    }

    return ret;
}

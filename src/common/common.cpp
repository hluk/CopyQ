/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "common/common.h"

#include "common/log.h"
#include "common/mimetypes.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QImage>
#include <QLocale>
#include <QMimeData>
#include <QObject>
#include <QTemporaryFile>
#include <QThread>
#include <QUrl>
#if QT_VERSION < 0x050000
#   include <QTextDocument> // Qt::escape()
#endif

namespace {

QString getImageFormatFromMime(const QString &mime)
{
    static const QString imageMimePrefix("image/");
    return mime.startsWith(imageMimePrefix) ? mime.mid(imageMimePrefix.length()).toUpper()
                                            : QString();
}

/**
 * Sometimes only Qt internal image data are available in cliboard,
 * so this tries to convert the image data (if available) to given MIME format.
 */
void cloneImageData(const QMimeData &data, const QString &mime, QVariantMap *dataMap)
{
    if ( !data.hasImage() )
        return;

    const QString fmt = getImageFormatFromMime(mime);
    const QImage image = data.imageData().value<QImage>();

    QBuffer buffer;
    bool saved = image.save(&buffer, fmt.toUtf8().constData());

    COPYQ_LOG( QString("Converting image to \"%1\" format: %1")
               .arg(fmt)
               .arg(saved ? "Failed" : "Done") );

    if (saved)
        dataMap->insert(mime, buffer.buffer());
}

bool setImageData(const QVariantMap &data, const QString &mime, QMimeData *mimeData)
{
    if ( !data.contains(mime) )
        return false;

    const QString imageFormat = getImageFormatFromMime(mime);
    if ( imageFormat.isEmpty() )
        return false;

    const QImage image = QImage::fromData( data.value(mime).toByteArray(), imageFormat.toUtf8().constData() );
    if ( image.isNull() )
        return false;

    mimeData->setImageData(image);
    return true;
}

} // namespace

QString quoteString(const QString &str)
{
#if QT_VERSION >= 0x040800
    return QLocale().quoteString(str);
#else
    return '"' + str + '"';
#endif
}

QString escapeHtml(const QString &str)
{
#if QT_VERSION < 0x050000
    return Qt::escape(str).replace('\n', "<br />");
#else
    return str.toHtmlEscaped().replace('\n', "<br />");
#endif
}

bool isMainThread()
{
    return QThread::currentThread() == QApplication::instance()->thread();
}

const QMimeData *clipboardData(QClipboard::Mode mode)
{
    Q_ASSERT( isMainThread() );
    COPYQ_LOG( QString("Getting %1 data.").arg(mode == QClipboard::Clipboard ? "clipboard"
                                                                             : "selection") );
    const QMimeData *data = QApplication::clipboard()->mimeData(mode);
    COPYQ_LOG(data != NULL ? "Got data." : "Data is NULL!");
    return data;
}

uint hash(const QVariantMap &data)
{
    uint hash = 0;

    foreach ( const QString &mime, data.keys() ) {
        // Skip some special data.
        if (mime == mimeWindowTitle || mime == mimeOwner)
            continue;
#ifdef COPYQ_WS_X11
        if (mime == mimeClipboardMode)
            continue;
#endif
        hash ^= qHash(data[mime].toByteArray()) + qHash(mime);
    }

    return hash;
}

QByteArray getUtf8Data(const QMimeData &data, const QString &format)
{
    if (format == mimeHtml)
        return data.html().toUtf8();
    if (format == mimeText)
        return data.text().toUtf8();
    if (format == mimeUriList) {
        QByteArray bytes;
        foreach ( const QUrl &url, data.urls() ) {
            if ( !bytes.isEmpty() )
                bytes += '\n';
            bytes += url.toString();
        }
        return bytes;
    }
    return data.data(format);
}

QString getTextData(const QVariantMap &data, const QString &mime)
{
    return data.contains(mime) ? QString::fromUtf8( data[mime].toByteArray() ) : QString();
}

QString getTextData(const QVariantMap &data)
{
    return getTextData(data, data.contains(mimeText) ? mimeText : mimeUriList);
}

void setTextData(QVariantMap *data, const QString &text, const QString &mime)
{
    data->insert(mime, text.toUtf8());
}

void setTextData(QVariantMap *data, const QString &text)
{
    setTextData(data, text, mimeText);
}

QVariantMap cloneData(const QMimeData &data, const QStringList &formats)
{
    static const QStringList internalMimeTypes = QStringList()
            << mimeOwner << mimeWindowTitle << mimeItemNotes;

    QVariantMap newdata;

    foreach (const QString &mime, formats) {
        const QByteArray bytes = getUtf8Data(data, mime);
        if ( !bytes.isEmpty() )
            newdata.insert(mime, bytes);
        else
            cloneImageData(data, mime, &newdata);
    }

    foreach (const QString &internalMime, internalMimeTypes) {
        if ( data.hasFormat(internalMime) )
            newdata.insert( internalMime, data.data(internalMime) );
    }

    if ( hasLogLevel(LogTrace) ) {
        foreach (const QString &format, data.formats()) {
            if ( !formats.contains(format) )
                COPYQ_LOG_VERBOSE(QString("Skipping format: %1").arg(format));
        }
    }

    return newdata;
}

QVariantMap cloneData(const QMimeData &data)
{
    QVariantMap newdata;
    foreach ( const QString &mime, data.formats() ) {
        // ignore uppercase mimetypes (e.g. UTF8_STRING, TARGETS, TIMESTAMP)
        // and internal type to check clipboard owner
        if ( !mime.isEmpty() && mime[0].isLower() )
            newdata.insert(mime, data.data(mime));
    }
    return newdata;
}

QMimeData* createMimeData(const QVariantMap &data)
{
    QStringList copyFormats = data.keys();
#ifdef COPYQ_WS_X11
    copyFormats.removeOne(mimeClipboardMode);
#endif

    QScopedPointer<QMimeData> newClipboardData(new QMimeData);

    foreach ( const QString &format, copyFormats )
        newClipboardData->setData( format, data[format].toByteArray() );

#ifdef HAS_TESTS
    // Don't set clipboard owner if monitor is only used to set clipboard for tests.
    if ( !QCoreApplication::instance()->property("CopyQ_testing").toBool() )
#endif
        newClipboardData->setData( mimeOwner, qgetenv("COPYQ_SESSION_NAME") );

    // Set image data.
    const QStringList formats =
            QStringList() << "image/png" << "image/bmp" << "application/x-qt-image" << data.keys();
    foreach (const QString &imageFormat, formats) {
        if ( setImageData(data, imageFormat, newClipboardData.data()) )
            break;
    }

    return newClipboardData.take();
}

QVariantMap createDataMap(const QString &format, const QVariant &value)
{
    QVariantMap dataMap;
    dataMap.insert(format, value);
    return dataMap;
}

QVariantMap createDataMap(const QString &format, const QByteArray &value)
{
    return createDataMap( format, QVariant(value) );
}

QVariantMap createDataMap(const QString &format, const QString &value)
{
    return createDataMap( format, value.toUtf8() );
}

bool ownsClipboardData(const QVariantMap &data)
{
    return data.contains(mimeOwner);
}

QString elideText(const QString &text, const QFont &font, const QString &format,
                  bool escapeAmpersands, int maxWidthPixels, int maxLines)
{
    const int oldLines = text.count('\n');

    QString newText = text;
    newText.remove(QRegExp("^\\s+"));

    const int newLines = newText.count('\n');

    int lines = 0;
    QString result;
    foreach ( QString line, newText.split('\n') ) {
        if (++lines > maxLines) {
            result.append( QString("...") );
            break;
        }

        // Show triple-dot in middle if text is too long.
        QFontMetrics fm(font);
        const int formatWidth = format.isEmpty() ? 0 : fm.width(format.arg(QString()));
        line = fm.elidedText(line.simplified(), Qt::ElideMiddle, maxWidthPixels - formatWidth);

        if ( !line.isEmpty() ) {
            if ( !result.isEmpty() )
                result.append('\n');
            result.append(line);
        }
    }

    // Escape all ampersands.
    if (escapeAmpersands)
        result.replace( QChar('&'), QString("&&") );

    if (newLines < oldLines && result != "...")
        result.prepend("...");

    return format.isEmpty() ? result : format.arg(result);
}

QString textLabelForData(const QVariantMap &data, const QFont &font, const QString &format,
                         bool escapeAmpersands, int maxWidthPixels, int maxLines)
{
    QString label;

    QStringList formats;
    foreach ( const QString &format, data.keys() ) {
        if ( !format.startsWith(MIME_PREFIX) )
            formats.append(format);
    }


    if ( formats.contains(mimeText) ) {
        const QString text = getTextData(data);
        const int n = text.count(QChar('\n')) + 1;

        if (n > 1)
            label = QObject::tr("%1 (%n lines)", "Label for multi-line text in clipboard", n);
        else
            label = QString("%1");

        label = label.arg( quoteString("%1") );

        if (!format.isEmpty())
            label = format.arg(label);

        return elideText(text, font, label, escapeAmpersands, maxWidthPixels, maxLines);
    } else if ( formats.indexOf(QRegExp("^image/.*")) != -1 ) {
        label = QObject::tr("<IMAGE>", "Label for image in clipboard");
    } else if ( formats.indexOf(mimeUriList) != -1 ) {
        label = QObject::tr("<FILES>", "Label for URLs/files in clipboard");
    } else if ( formats.isEmpty() ) {
        label = QObject::tr("<EMPTY>", "Label for empty clipboard");
    } else {
        label = QObject::tr("<DATA>", "Label for data in clipboard");
    }

    if (!format.isEmpty())
        label = format.arg(label);

    return label;
}

QString shortcutToRemove()
{
#ifdef Q_OS_MAC
    return QObject::tr("Backspace", "Key to remove item or MIME on OS X");
#else
    return QObject::tr("Delete", "Key to remove item or MIME");
#endif
}

void renameToUnique(QString *name, const QStringList &names)
{
    const QString baseName = *name;
    int i = 0;
    while ( names.contains(*name) )
        *name = baseName + " (" + QString::number(++i) + ')';
}

bool containsData(const QVariantMap &data)
{
    foreach ( const QString &mime, data.keys() ) {
        if (mime != mimeOwner
                && mime != mimeWindowTitle
        #ifdef COPYQ_WS_X11
                && mime != mimeClipboardMode
        #endif
                && mime != mimeItems)
        {
            return true;
        }
    }

    return false;
}

bool openTemporaryFile(QTemporaryFile *file)
{
    const QString tmpFileName = QString("CopyQ.XXXXXX.ini");
    const QString tmpPath = QDir( QDir::tempPath() ).absoluteFilePath(tmpFileName);

    file->setFileTemplate(tmpPath);
    file->setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    return file->open();
}

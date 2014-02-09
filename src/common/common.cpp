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

#include "common.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QLocale>
#include <QMimeData>
#include <QObject>
#include <QThread>
#include <QUrl>
#if QT_VERSION < 0x050000
#   include <QTextDocument> // Qt::escape()
#endif

const char mimeText[] = "text/plain";
const char mimeHtml[] = "text/html";
const char mimeUriList[] = "text/uri-list";
const char mimeWindowTitle[] = MIME_PREFIX "owner-window-title";
const char mimeItems[] = MIME_PREFIX "item";
const char mimeItemNotes[] = MIME_PREFIX "item-notes";
const char mimeApplicationSettings[] = MIME_PREFIX "settings";
const char mimeMessage[] = MIME_PREFIX "message";
const char mimeOwner[] = MIME_PREFIX "owner";
#ifdef COPYQ_WS_X11
const char mimeClipboardMode[] = MIME_PREFIX "clipboard-mode";
#endif

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

QString createLogMessage(const QString &label, const QString &text, const LogLevel level)
{
    QString levelId;

    if (level == LogNote)
        levelId = QString("%1");
    else if (level == LogWarning)
        levelId = QObject::tr("warning: %1");
    else if (level == LogError)
        levelId = QObject::tr("ERROR: %1");
#ifdef COPYQ_LOG_DEBUG
    else if (level == LogDebug)
        levelId = QString("DEBUG: %1");
#endif

    return label + " " + levelId.arg(text) + "\n";
}

void log(const QString &text, const LogLevel level)
{
    const QString msg = createLogMessage("CopyQ", text, level);

    QFile f;
    f.open(stderr, QIODevice::WriteOnly);
    f.write( msg.toLocal8Bit() );
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
        if (mime == mimeWindowTitle)
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
    return getTextData(data, mimeText);
}

void setTextData(QVariantMap *data, const QString &text)
{
    data->insert(mimeText, text.toUtf8());
}

QVariantMap cloneData(const QMimeData &data, const QStringList &formats)
{
    QVariantMap newdata;
    foreach (const QString &mime, formats) {
        const QByteArray bytes = getUtf8Data(data, mime);
        if ( !bytes.isEmpty() )
            newdata.insert(mime, bytes);
    }

    if (data.hasFormat(mimeOwner))
        newdata.insert(mimeOwner, data.data(mimeOwner));

#ifdef COPYQ_LOG_DEBUG
    foreach (const QString &format, data.formats()) {
        if (!formats.contains(format) && format != mimeOwner) {
            COPYQ_LOG(QString("skipping format: %1").arg(format));
        }
    }
#endif // COPYQ_LOG_DEBUG

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
    QScopedPointer<QMimeData> newClipboardData(new QMimeData);
    foreach ( const QString &format, data.keys() )
        newClipboardData->setData( format, data[format].toByteArray() );
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

    const QStringList formats = data.keys();
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
    } else if ( formats.isEmpty() || (formats.size() == 1 && formats[0] == mimeWindowTitle) ) {
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

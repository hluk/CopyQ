/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include <QDesktopWidget>
#include <QDir>
#include <QImage>
#include <QKeyEvent>
#include <QLocale>
#include <QMimeData>
#include <QObject>
#include <QPoint>
#include <QStyle>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QWidget>
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

QImage getImageData(const QMimeData &data)
{
    // NOTE: Application hangs if using mulitple sessions and
    //       calling QMimeData::hasImage() on X11 clipboard.
    COPYQ_LOG("Fetching image data from clipboard");
    const QImage image = data.imageData().value<QImage>();
    COPYQ_LOG( QString("Image is %1").arg(image.isNull() ? "invalid" : "valid") );
    return image;
}

/**
 * Sometimes only Qt internal image data are available in cliboard,
 * so this tries to convert the image data (if available) to given format.
 */
void cloneImageData(
        const QImage &image, const QString &format,
        const QString &mime, QVariantMap *dataMap)
{
    if (image.isNull())
        return;

    QBuffer buffer;
    bool saved = image.save(&buffer, format.toUtf8().constData());

    COPYQ_LOG( QString("Converting image to \"%1\" format: %2")
               .arg(format)
               .arg(saved ? "Done" : "Failed") );

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

QTextCodec *codecForText(const QByteArray &bytes)
{
    // Guess unicode codec for text if BOM is missing.
    if (bytes.size() >= 2 && bytes.size() % 2 == 0) {
        if (bytes.size() >= 4 && bytes.size() % 4 == 0) {
            if (bytes.at(0) == 0 && bytes.at(1) == 0)
                return QTextCodec::codecForName("utf-32be");
            if (bytes.at(2) == 0 && bytes.at(3) == 0)
                return QTextCodec::codecForName("utf-32le");
        }

        if (bytes.at(0) == 0)
            return QTextCodec::codecForName("utf-16be");

        if (bytes.at(1) == 0)
            return QTextCodec::codecForName("utf-16le");
    }

    return QTextCodec::codecForName("utf-8");
}

int indexOfKeyHint(const QString &name)
{
    bool amp = false;
    int i = 0;

    foreach (const QChar &c, name) {
        if (c == '&')
            amp = !amp;
        else if (amp)
            return i - 1;
        ++i;
    }

    return -1;
}


QString escapeHtmlSpaces(const QString &str)
{
    QString str2 = str;
    return str2
            .replace(' ', "&nbsp;")
            .replace('\n', "<br />");
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
    return escapeHtmlSpaces(Qt::escape(str));
#else
    return escapeHtmlSpaces(str.toHtmlEscaped());
#endif
}

bool isMainThread()
{
    return QThread::currentThread() == qApp->thread();
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
    if (format == mimeText || format == mimeHtml)
        return dataToText( data.data(format), format ).toUtf8();

    if (format == mimeUriList) {
        QByteArray bytes;
        foreach ( const QUrl &url, data.urls() ) {
            if ( !bytes.isEmpty() )
                bytes += '\n';
            bytes += url.toString().toUtf8();
        }
        return bytes;
    }

    return data.data(format);
}

QString getTextData(const QByteArray &bytes)
{
    // QString::fromUtf8(bytes) ends string at first '\0'.
    return QString::fromUtf8( bytes.constData(), bytes.size() );
}

QString getTextData(const QVariantMap &data, const QString &mime)
{
    return getTextData( data.value(mime).toByteArray() );
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
            << mimeOwner << mimeWindowTitle << mimeItemNotes << mimeHidden;

    QVariantMap newdata;

    QImage image;
    bool imageLoaded = false;

    foreach (const QString &mime, formats) {
        const QByteArray bytes = getUtf8Data(data, mime);
        if ( !bytes.isEmpty() ) {
            newdata.insert(mime, bytes);
        } else if ( !imageLoaded || !image.isNull() ) {
            const QString format = getImageFormatFromMime(mime);
            if ( !format.isEmpty() ) {
                if (!imageLoaded) {
                    image = getImageData(data);
                    imageLoaded = true;
                }
                cloneImageData(image, format, mime, &newdata);
            }
        }
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
    QStringList formats;

    foreach ( const QString &mime, data.formats() ) {
        // ignore uppercase mimetypes (e.g. UTF8_STRING, TARGETS, TIMESTAMP)
        // and internal type to check clipboard owner
        if ( !mime.isEmpty() && mime[0].isLower() )
            formats.append(mime);
    }

    return cloneData(data, formats);
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
    if ( !qApp->property("CopyQ_testing").toBool() )
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
    if (maxWidthPixels <= 0)
        maxWidthPixels = smallIconSize() * 20;

    QStringList lines = text.split('\n');

    // Ignore empty lines at beginning.
    const QRegExp reNonEmpty(".*\\S.*");
    const int firstLine = qMax(0, lines.indexOf(reNonEmpty));
    const int lastLine = qMax(0, lines.lastIndexOf(reNonEmpty, firstLine + maxLines - 1));

    // If empty lines are at beginning, prepend triple dot.
    if (firstLine != 0)
        lines[firstLine].prepend("...");

    // If there are too many lines, append triple dot.
    if (lastLine + 1 != lines.size())
        lines[lastLine].append("...");

    lines = lines.mid(firstLine, lastLine - firstLine + 1);

    QFontMetrics fm(font);
    const int formatWidth = format.isEmpty() ? 0 : fm.width(format.arg(QString()));

    // Remove redundant spaces from single line text.
    if (lines.size() == 1)
        lines[0] = lines[0].simplified();

    // Find common indentation.
    int commonIndent = lines.value(0).size();
    const QRegExp reNonSpace("\\S");
    for (int i = 0; i < lines.size(); ++i) {
        const int lineIndent = lines[i].indexOf(reNonSpace);
        if (lineIndent != -1 && lineIndent < commonIndent) {
            commonIndent = lineIndent;
            if (commonIndent == 0)
                break;
        }
    }

    // Remove common indentation each line and elide text if too long.
    for (int i = 0; i < lines.size(); ++i) {
        QString &line = lines[i];
        line = fm.elidedText(line.mid(commonIndent), Qt::ElideMiddle, maxWidthPixels - formatWidth);
    }

    QString result = lines.join("\n");

    // Escape all ampersands.
    if (escapeAmpersands)
        result.replace( QChar('&'), QString("&&") );

    return format.isEmpty() ? result : format.arg(result);
}

QString textLabelForData(const QVariantMap &data, const QFont &font, const QString &format,
                         bool escapeAmpersands, int maxWidthPixels, int maxLines)
{
    QString label;

    const QStringList formats = data.keys();

    const QString notes = data.value(mimeItemNotes).toString();

    if ( data.contains(mimeHidden) ) {
        label = QObject::tr("<HIDDEN>", "Label for hidden/secret clipboard content");
    } else if ( formats.contains(mimeText) ) {
        const QString text = getTextData(data);
        const int n = text.count(QChar('\n')) + 1;

        if (n > 1)
            label = QObject::tr("%1 (%n lines)", "Label for multi-line text in clipboard", n);
        else
            label = QString("%1");

        if (!format.isEmpty())
            label = format.arg(label);

        const QString textWithNotes = notes.isEmpty() ? text : notes + ": " + text;
        return elideText(textWithNotes, font, label, escapeAmpersands, maxWidthPixels, maxLines);
    } else if ( formats.indexOf(QRegExp("^image/.*")) != -1 ) {
        label = QObject::tr("<IMAGE>", "Label for image in clipboard");
    } else if ( formats.indexOf(mimeUriList) != -1 ) {
        label = QObject::tr("<FILES>", "Label for URLs/files in clipboard");
    } else if ( formats.isEmpty() ) {
        label = QObject::tr("<EMPTY>", "Label for empty clipboard");
    } else {
        label = QObject::tr("<DATA>", "Label for data in clipboard");
    }

    if (!notes.isEmpty()) {
        label = elideText(notes, font, QString(), escapeAmpersands, maxWidthPixels, maxLines)
                + ": " + label;
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

QString portableShortcutText(const QKeySequence &shortcut)
{
    // WORKAROUND: Qt has convert some keys to upper case which
    //             breaks some shortcuts on some keyboard layouts.
    return shortcut.toString(QKeySequence::PortableText).toLower();
}

QString toPortableShortcutText(const QString &shortcutNativeText)
{
    return portableShortcutText(
                QKeySequence(shortcutNativeText, QKeySequence::NativeText));
}

void renameToUnique(QString *name, const QStringList &names)
{
    const QString baseName = *name;
    int i = 0;
    while ( names.contains(*name) )
        *name = baseName + " (" + QString::number(++i) + ')';
}

bool containsAnyData(const QVariantMap &data)
{
    foreach ( const QString &mime, data.keys() ) {
        if (mime != mimeOwner
                && mime != mimeWindowTitle
                && mime != mimeHidden
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

int pointsToPixels(int points)
{
    return points * QApplication::desktop()->physicalDpiX() / 72;
}

void initSingleShotTimer(QTimer *timer, int milliseconds, const QObject *object, const char *slot)
{
    timer->setSingleShot(true);
    timer->setInterval(milliseconds);
    if (object && slot)
        QObject::connect( timer, SIGNAL(timeout()), object, slot );
}

QString dataToText(const QByteArray &bytes, const QString &mime)
{
    bool isHtml = (mime == "text/html");
    QTextCodec *defaultCodec = codecForText(bytes);
    QTextCodec *codec = isHtml
            ? QTextCodec::codecForHtml(bytes, defaultCodec)
            : QTextCodec::codecForUtfText(bytes, defaultCodec);

    return codec->toUnicode(bytes);
}

QByteArray readTemporaryFileContent(const QTemporaryFile &file)
{
    // Open temporary file with other QFile instance so the file cache is up-to-date.
    QFile file2(file.fileName());
    return file2.open(QIODevice::ReadOnly) ? file2.readAll() : QByteArray();
}

bool clipboardContains(QClipboard::Mode mode, const QVariantMap &data)
{
    const QMimeData *clipboardData = ::clipboardData(mode);
    if (!clipboardData)
        return false;

    foreach ( const QString &format, data.keys() ) {
        if ( !format.startsWith(COPYQ_MIME_PREFIX)
             && data.value(format).toByteArray() != getUtf8Data(*clipboardData, format) )
        {
            return false;
        }
    }

    return true;
}

bool isClipboardData(const QVariantMap &data)
{
    return data.value(mimeClipboardMode).toByteArray().isEmpty();
}

int smallIconSize()
{
    return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
}

QPoint toScreen(const QPoint &pos, int w, int h)
{
    const QRect availableGeometry = QApplication::desktop()->availableGeometry(pos);
    return QPoint(
                qMax(0, qMin(pos.x(), availableGeometry.right() - w)),
                qMax(0, qMin(pos.y(), availableGeometry.bottom() - h))
                );
}

bool hasKeyHint(const QString &name)
{
    return indexOfKeyHint(name) != -1;
}

QString removeKeyHint(QString &name)
{
    const int i = indexOfKeyHint(name);
    return i == -1 ? name : name.remove(i, 1);
}

void moveWindowOnScreen(QWidget *w, const QPoint &pos)
{
    const QRect availableGeometry = QApplication::desktop()->availableGeometry(pos);
    const int x = qMax(0, qMin(pos.x(), availableGeometry.right() - w->width()));
    const int y = qMax(0, qMin(pos.y(), availableGeometry.bottom() - w->height()));
    w->move(x, y);
    moveToCurrentWorkspace(w);
}

void moveToCurrentWorkspace(QWidget *w)
{
#ifdef COPYQ_WS_X11
    /* Re-initialize window in window manager so it can popup on current workspace. */
    if (w->isVisible()) {
        w->hide();
        w->show();
    }
#else
    Q_UNUSED(w);
#endif
}

bool handleViKey(QKeyEvent *event, QObject *eventReceiver)
{
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    switch ( key ) {
    case Qt::Key_G:
        key = mods & Qt::ShiftModifier ? Qt::Key_End : Qt::Key_Home;
        mods = mods & ~Qt::ShiftModifier;
        break;
    case Qt::Key_J:
        key = Qt::Key_Down;
        break;
    case Qt::Key_K:
        key = Qt::Key_Up;
        break;
    case Qt::Key_F:
    case Qt::Key_D:
    case Qt::Key_B:
    case Qt::Key_U:
        if (mods & Qt::ControlModifier) {
            key = (key == Qt::Key_F || key == Qt::Key_D) ? Qt::Key_PageDown : Qt::Key_PageUp;
            mods = mods & ~Qt::ControlModifier;
        } else {
            return false;
        }
        break;
    case Qt::Key_BracketLeft:
        if (mods & Qt::ControlModifier) {
            key = Qt::Key_Escape;
            mods = mods & ~Qt::ControlModifier;
        } else {
            return false;
        }
        break;
    default:
        return false;
    }

    QKeyEvent event2(QEvent::KeyPress, key, mods, event->text());
    QCoreApplication::sendEvent(eventReceiver, &event2);
    event->accept();

    return true;
}

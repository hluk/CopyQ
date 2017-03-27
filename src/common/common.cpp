/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDropEvent>
#include <QDir>
#include <QImage>
#include <QImageWriter>
#include <QKeyEvent>
#include <QMimeData>
#include <QMovie>
#include <QObject>
#include <QPoint>
#include <QProcess>
#include <QStyle>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QWidget>

// This is needed on X11 when retrieving lots of data from clipboard.
#if QT_VERSION >= 0x050000 && defined(COPYQ_WS_X11)
#   define PROCESS_EVENTS_BEFORE_CLIPBOARD_DATA
#endif

#include <algorithm>
#include <memory>

namespace {

QString getImageFormatFromMime(const QString &mime)
{
    const auto imageMimePrefix = "image/";
    const auto prefixLength = static_cast<int>(strlen(imageMimePrefix));
    return mime.startsWith(imageMimePrefix) ? mime.mid(prefixLength) : QString();
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

    // Omit converting unsupported formats (takes too much time and still fails).
    if ( !QImageWriter::supportedImageFormats().contains(format.toUtf8()) )
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

    QByteArray bytes = data.value(mime).toByteArray();

    // Omit converting animated images to static ones.
    QBuffer buffer(&bytes);
    QMovie animatedImage( &buffer, imageFormat.toUtf8().constData() );
    if ( animatedImage.frameCount() > 1 )
        return false;

    const QImage image = QImage::fromData( bytes, imageFormat.toUtf8().constData() );
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

QByteArray getUtf8Data(const QMimeData &data, const QString &format)
{
    if (format == mimeText || format == mimeHtml)
        return dataToText( data.data(format), format ).toUtf8();

    if (format == mimeUriList) {
        QByteArray bytes;
        for ( const auto &url : data.urls() ) {
            if ( !bytes.isEmpty() )
                bytes += '\n';
            bytes += url.toString().toUtf8();
        }
        return bytes;
    }

    return data.data(format);
}

} // namespace

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
    COPYQ_LOG(data != nullptr ? "Got data." : "Data is nullptr!");
    return data;
}

QVariantMap cloneData(const QMimeData &data, QStringList formats)
{
    const auto internalMimeTypes = {mimeOwner, mimeWindowTitle, mimeItemNotes, mimeHidden};

    QVariantMap newdata;

    QImage image;
    bool imageLoaded = false;

    // Ignore non-text data if text is available.
    if ( formats.contains(mimeText) && data.hasFormat(mimeText) ) {
        const auto first = std::remove_if(
                    std::begin(formats), std::end(formats),
                    [](const QString &format) {
                        return !format.startsWith(COPYQ_MIME_PREFIX)
                            && !format.startsWith("text/");
                    });
        formats.erase(first, std::end(formats));
    }

#ifdef PROCESS_EVENTS_BEFORE_CLIPBOARD_DATA
    const QPointer<const QMimeData> dataGuard(&data);
#endif

    for (const auto &mime : formats) {
#ifdef PROCESS_EVENTS_BEFORE_CLIPBOARD_DATA
        QCoreApplication::processEvents();
        if (dataGuard.isNull()) {
            log("Clipboard data lost", LogWarning);
            return newdata;
        }
#endif

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

    for (const auto &internalMime : internalMimeTypes) {
        if ( data.hasFormat(internalMime) )
            newdata.insert( internalMime, data.data(internalMime) );
    }

    if ( hasLogLevel(LogTrace) ) {
        for (const auto &format : data.formats()) {
            if ( !formats.contains(format) )
                COPYQ_LOG_VERBOSE(QString("Skipping format: %1").arg(format));
        }
    }

    return newdata;
}

QVariantMap cloneData(const QMimeData &data)
{
    QStringList formats;

    for ( const auto &mime : data.formats() ) {
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
    copyFormats.removeOne(mimeClipboardMode);

    std::unique_ptr<QMimeData> newClipboardData(new QMimeData);

    for ( const auto &format : copyFormats )
        newClipboardData->setData( format, data[format].toByteArray() );

#ifdef HAS_TESTS
    // Don't set clipboard owner if monitor is only used to set clipboard for tests.
    if ( !qApp->property("CopyQ_testing").toBool() )
#endif
        newClipboardData->setData( mimeOwner, qgetenv("COPYQ_SESSION_NAME") );

    // Set image data.
    const QStringList formats =
            QStringList() << "image/png" << "image/bmp" << "application/x-qt-image" << data.keys();
    for (const auto &imageFormat : formats) {
        if ( setImageData(data, imageFormat, newClipboardData.get()) )
            break;
    }

    return newClipboardData.release();
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
    for (const auto &line : lines) {
        const int lineIndent = line.indexOf(reNonSpace);
        if (lineIndent != -1 && lineIndent < commonIndent) {
            commonIndent = lineIndent;
            if (commonIndent == 0)
                break;
        }
    }

    // Remove common indentation each line and elide text if too long.
    for (auto &line : lines)
        line = fm.elidedText(line.mid(commonIndent), Qt::ElideMiddle, maxWidthPixels - formatWidth);

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

void renameToUnique(QString *name, const QStringList &names)
{
    const QString baseName = *name;
    int i = 0;
    while ( names.contains(*name) )
        *name = baseName + " (" + QString::number(++i) + ')';
}

bool containsAnyData(const QVariantMap &data)
{
    for ( const auto &mime : data.keys() ) {
        if (mime != mimeOwner
                && mime != mimeWindowTitle
                && mime != mimeHidden
                && mime != mimeClipboardMode
                && mime != mimeItems)
        {
            return true;
        }
    }

    return false;
}

bool openTemporaryFile(QTemporaryFile *file, const QString &suffix)
{
    const QString tmpFileName = "CopyQ.XXXXXX" + suffix;
    const QString tmpPath = QDir( QDir::tempPath() ).absoluteFilePath(tmpFileName);

    file->setFileTemplate(tmpPath);
    file->setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    return file->open();
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
    auto codec = (mime == mimeHtml)
            ? QTextCodec::codecForHtml(bytes, nullptr)
            : QTextCodec::codecForUtfText(bytes, nullptr);

    if (!codec)
        codec = codecForText(bytes);

    return codec->toUnicode(bytes);
}

bool clipboardContains(QClipboard::Mode mode, const QVariantMap &data)
{
    const QMimeData *clipboardData = ::clipboardData(mode);
    if (!clipboardData)
        return false;

    for ( const auto &format : data.keys() ) {
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
    case Qt::Key_L:
        key = Qt::Key_Return;
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

void terminateProcess(QProcess *p)
{
    if (p->state() == QProcess::NotRunning)
        return;

    p->terminate();
    if ( p->state() != QProcess::NotRunning && !p->waitForFinished(5000) ) {
        p->kill();
        p->waitForFinished(5000);
    }
}

bool canDropToTab(const QDropEvent &event)
{
    const auto &data = *event.mimeData();
    return data.hasFormat(mimeItems) || data.hasText() || data.hasImage() || data.hasUrls();
}

void acceptDrag(QDropEvent *event)
{
    // Default drop action in item list and tab bar/tree should be "move."
    if ( event->possibleActions().testFlag(Qt::MoveAction)
         && event->mimeData()->hasFormat(mimeItems)
         // WORKAROUND: Test currently pressed modifiers instead of the ones in event (QTBUG-57168).
         && !QApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier) )
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->acceptProposedAction();
    }
}

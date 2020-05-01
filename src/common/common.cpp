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
#include <QElapsedTimer>
#include <QImage>
#include <QImageWriter>
#include <QKeyEvent>
#include <QMimeData>
#include <QMovie>
#include <QObject>
#include <QPoint>
#include <QProcess>
#include <QRegularExpression>
#include <QTextCodec>
#include <QThread>
#include <QUrl>
#include <QWidget>

#ifdef COPYQ_WS_X11
# include "platform/x11/x11platform.h"
# include <QTimer>
#endif

#include <algorithm>
#include <memory>

namespace {

const int maxElidedTextLineLength = 512;

#ifdef COPYQ_WS_X11
// WORKAROUND: This fixes stuck clipboard access by creating dummy X11 events
//             when accessing clipboard takes too long.
class WakeUpThread final {
public:
    WakeUpThread()
    {
        m_timerWakeUp.setInterval(100);
        QObject::connect( &m_timerWakeUp, &QTimer::timeout, []() {
            sendDummyX11Event();
        });

        m_timerWakeUp.moveToThread(&m_wakeUpThread);
        QObject::connect( &m_wakeUpThread, &QThread::started,
                          &m_timerWakeUp, [this]() { m_timerWakeUp.start(); } );
        QObject::connect( &m_wakeUpThread, &QThread::finished,
                          &m_timerWakeUp, &QTimer::stop );
        m_wakeUpThread.start();
    }

    ~WakeUpThread()
    {
        m_wakeUpThread.quit();
        m_wakeUpThread.wait();
    }

private:
    QTimer m_timerWakeUp;
    QThread m_wakeUpThread;
};
#endif

class MimeData final : public QMimeData {
protected:
    QVariant retrieveData(const QString &mimeType, QVariant::Type preferredType) const override {
        COPYQ_LOG_VERBOSE( QString("Providing \"%1\"").arg(mimeType) );
        return QMimeData::retrieveData(mimeType, preferredType);
    }
};

// Avoids accessing old clipboard/drag'n'drop data.
class ClipboardDataGuard final {
public:
    class ElapsedGuard {
    public:
        explicit ElapsedGuard(const QString &format)
            : m_format(format)
        {
            COPYQ_LOG_VERBOSE( QString("Accessing \"%1\"").arg(format) );
            m_elapsed.start();
        }

        ~ElapsedGuard()
        {
            const auto t = m_elapsed.elapsed();
            if (t > 500)
                log( QString("ELAPSED %1 ms acessing \"%2\"").arg(t).arg(m_format), LogWarning );
        }
    private:
        QString m_format;
        QElapsedTimer m_elapsed;
    };

    explicit ClipboardDataGuard(const QMimeData &data, bool *abortCloning = nullptr)
        : m_dataGuard(&data)
        , m_abort(abortCloning)
    {
        m_timerExpire.start();
    }

    bool hasFormat(const QString &mime)
    {
        ElapsedGuard _("has:" + mime);
        return refresh() && m_dataGuard->hasFormat(mime);
    }

    QByteArray data(const QString &mime)
    {
        ElapsedGuard _(mime);
        return refresh() ? m_dataGuard->data(mime) : QByteArray();
    }

    QList<QUrl> urls()
    {
        ElapsedGuard _("urls");
        return refresh() ? m_dataGuard->urls() : QList<QUrl>();
    }

    QString text()
    {
        ElapsedGuard _("text");
        return refresh() ? m_dataGuard->text() : QString();
    }

    bool hasText()
    {
        ElapsedGuard _("hasText");
        return refresh() && m_dataGuard->hasText();
    }

    QImage getImageData()
    {
        ElapsedGuard _("imageData");
        if (!refresh())
            return QImage();

        // NOTE: Application hangs if using mulitple sessions and
        //       calling QMimeData::hasImage() on X11 clipboard.
        COPYQ_LOG_VERBOSE("Fetching image data from clipboard");
        const QImage image = m_dataGuard->imageData().value<QImage>();
        COPYQ_LOG_VERBOSE( QString("Image is %1").arg(image.isNull() ? "invalid" : "valid") );
        return image;
    }

    QByteArray getUtf8Data(const QString &format)
    {
        ElapsedGuard _("UTF8:" + format);
        if (!refresh())
            return QByteArray();

        if (format == mimeUriList) {
            QByteArray bytes;
            for ( const auto &url : urls() ) {
                if ( !bytes.isEmpty() )
                    bytes += '\n';
                bytes += url.toString().toUtf8();
            }
            return bytes;
        }

        if ( format == mimeText && !hasFormat(mimeText) )
            return text().toUtf8();

        if ( format.startsWith("text/") )
            return dataToText( data(format), format ).toUtf8();

        return data(format);
    }

private:
    bool refresh()
    {
        if (m_abort && *m_abort)
            return false;

        if (m_dataGuard.isNull())
            return false;

        const auto elapsed = m_timerExpire.elapsed();
        if (elapsed > 5000) {
            log("Clipboard data expired, refusing to access old data", LogWarning);
            m_dataGuard = nullptr;
            return false;
        }

        if (elapsed > 100)
            QCoreApplication::processEvents();

        return !m_dataGuard.isNull();
    }

    QPointer<const QMimeData> m_dataGuard;
    QElapsedTimer m_timerExpire;
    bool *m_abort = nullptr;

#ifdef COPYQ_WS_X11
    WakeUpThread m_wakeUpThread;
#endif
};

QString getImageFormatFromMime(const QString &mime)
{
    const auto imageMimePrefix = "image/";
    const auto prefixLength = static_cast<int>(strlen(imageMimePrefix));
    return mime.startsWith(imageMimePrefix) ? mime.mid(prefixLength) : QString();
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
               .arg(format,
                    saved ? "Done" : "Failed") );

    if (saved)
        dataMap->insert(mime, buffer.buffer());
}

/**
 * Allow cloning images only with reasonable size.
 */
bool canCloneImageData(const QImage &image)
{
    return !image.isNull()
        && image.height() <= 4096
        && image.width() <= 4096;
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

bool findFormatsWithPrefix(bool hasPrefix, const QString &prefix, const QVariantMap &data)
{
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        if ( it.key().startsWith(prefix) == hasPrefix )
            return hasPrefix;
    }

    return !hasPrefix;
}

} // namespace

bool isMainThread()
{
    return QThread::currentThread() == qApp->thread();
}

const QMimeData *clipboardData(ClipboardMode mode)
{
    Q_ASSERT( isMainThread() );

    const auto modeText = mode == ClipboardMode::Clipboard ? "clipboard" : "selection";
    COPYQ_LOG_VERBOSE( QString("Getting %1 data.").arg(modeText) );

    const auto qtmode = mode == ClipboardMode::Clipboard
            ? QClipboard::Clipboard
            : QClipboard::Selection;

    const QMimeData *data = QGuiApplication::clipboard()->mimeData(qtmode);

    if (data)
        COPYQ_LOG_VERBOSE( QString("Got %1 data.").arg(modeText) );
    else
        log( QString("Null data in %1.").arg(modeText), LogError );

    return data;
}

QVariantMap cloneData(const QMimeData &rawData, QStringList formats, bool *abortCloning)
{
    ClipboardDataGuard data(rawData, abortCloning);

    const auto internalMimeTypes = {mimeOwner, mimeWindowTitle, mimeItemNotes, mimeHidden};

    QVariantMap newdata;

    /*
     Some apps provide images even when copying huge spreadsheet, this can
     block those apps while generating and providing the data.

     This code removes ignores any image data if text is available.

     Images in SVG and other XML formats are expected to be relatively small
     so these doesn't have to be ignored.
     */
    if ( formats.contains(mimeText) && data.hasText() ) {
        const QString mimeImagePrefix = "image/";
        const auto first = std::remove_if(
                    std::begin(formats), std::end(formats),
                    [&mimeImagePrefix](const QString &format) {
                        return format.startsWith(mimeImagePrefix)
                            && !format.contains("xml")
                            && !format.contains("svg");
                    });
        formats.erase(first, std::end(formats));
    }

    QStringList imageFormats;
    for (const auto &mime : formats) {
        const QByteArray bytes = data.getUtf8Data(mime);
        if ( bytes.isEmpty() )
            imageFormats.append(mime);
        else
            newdata.insert(mime, bytes);
    }

    for (const auto &internalMime : internalMimeTypes) {
        if ( data.hasFormat(internalMime) )
            newdata.insert( internalMime, data.data(internalMime) );
    }

    // Retrieve images last since this can take a while.
    if ( !imageFormats.isEmpty() ) {
        const QImage image = data.getImageData();
        if ( canCloneImageData(image) ) {
            for (const auto &mime : imageFormats) {
                const QString format = getImageFormatFromMime(mime);
                if ( !format.isEmpty() )
                    cloneImageData(image, format, mime, &newdata);
            }
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

    if ( !formats.contains(mimeText) ) {
        const QString textPrefix(QLatin1String(mimeText) + ";");
        bool containsText = false;
        for (int i = formats.size() - 1; i >= 0; --i) {
            if ( formats[i].startsWith(textPrefix) ) {
                formats.removeAt(i);
                containsText = true;
            }
        }
        if (containsText)
            formats.append(mimeText);
    }

    return cloneData(data, formats);
}

QMimeData* createMimeData(const QVariantMap &data)
{
    QStringList copyFormats = data.keys();
    copyFormats.removeOne(mimeClipboardMode);

    std::unique_ptr<QMimeData> newClipboardData(new MimeData);

    for ( const auto &format : copyFormats )
        newClipboardData->setData( format, data[format].toByteArray() );

    if ( !copyFormats.contains(mimeOwner) )
        newClipboardData->setData( mimeOwner, makeClipboardOwnerData() );

    // Set image data.
    const QStringList formats =
            QStringList() << "image/png" << "image/bmp" << "application/x-qt-image" << data.keys();
    for (const auto &imageFormat : formats) {
        if ( setImageData(data, imageFormat, newClipboardData.get()) )
            break;
    }

    return newClipboardData.release();
}

bool anySessionOwnsClipboardData(const QVariantMap &data)
{
    return data.contains(mimeOwner);
}

QString elideText(const QString &text, const QFont &font, const QString &format,
                  bool escapeAmpersands, int maxWidthPixels, int maxLines)
{
    if ( text.isEmpty() )
        return QString();

    if (maxWidthPixels <= 0)
        maxWidthPixels = smallIconSize() * 20;

    QStringList lines = text.split('\n');

    int firstLine = -1;
    int lastLine = -1;
    int commonIndent = text.size();
    static const QRegularExpression reNonSpace("\\S");
    for (int i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i];
        const int lineIndent = line.indexOf(reNonSpace);
        if (lineIndent == -1)
            continue;

        if (firstLine == -1)
            firstLine = i;

        lastLine = i;
        if (lineIndent < commonIndent)
            commonIndent = lineIndent;

        if (firstLine - lastLine + 1 >= maxLines)
            break;
    }

    if (lastLine == -1)
        return QString("...");

    // If there are too many lines, append triple dot.
    if (lastLine + 1 != lines.size())
        lines[lastLine].append("...");

    lines = lines.mid(firstLine, lastLine - firstLine + 1);

    QFontMetrics fm(font);
#if QT_VERSION >= QT_VERSION_CHECK(5,11,0)
    const int formatWidth = format.isEmpty() ? 0 : fm.horizontalAdvance(format.arg(QString()));
#else
    const int formatWidth = format.isEmpty() ? 0 : fm.width(format.arg(QString()));
#endif

    // Remove redundant spaces from single line text.
    if (lines.size() == 1) {
        lines[0] = lines[0].simplified();
        commonIndent = 0;
    }

    // Remove common indentation each line and elide text if too long.
    for (auto &line : lines) {
        line = line.mid(commonIndent);

        // Make eliding huge text faster.
        if (line.size() > maxElidedTextLineLength)
            line = line.left(maxElidedTextLineLength) + "...";

        line = fm.elidedText(line, Qt::ElideMiddle, maxWidthPixels - formatWidth);
    }

    // If empty lines are at beginning, prepend triple dot.
    if (firstLine != 0) {
        if (lines.size() == 1)
            lines.first().prepend("...");
        else
            lines.prepend("...");
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

    const QString notes = data.value(mimeItemNotes).toString();

    if ( data.contains(mimeHidden) ) {
        label = QObject::tr("<HIDDEN>", "Label for hidden/secret clipboard content");
    } else if ( data.contains(mimeText) || data.contains(mimeUriList) ) {
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
    } else if ( findFormatsWithPrefix(true, "image/", data) ) {
        label = QObject::tr("<IMAGE>", "Label for image in clipboard");
    } else if ( data.contains(mimeItems) ) {
        label = QObject::tr("<ITEMS>", "Label for copied items in clipboard");
    } else if ( findFormatsWithPrefix(false, COPYQ_MIME_PREFIX, data) ) {
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

QString textLabelForData(const QVariantMap &data)
{
    return textLabelForData(data, QFont(), QString());
}

void renameToUnique(QString *name, const QStringList &names)
{
    const QString baseName = *name;
    int i = 0;
    while ( names.contains(*name) )
        *name = baseName + " (" + QString::number(++i) + ')';
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
    case Qt::Key_X:
        key = Qt::Key_Delete;
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
         && event->mimeData()->hasFormat(mimeOwner)
         // WORKAROUND: Test currently pressed modifiers instead of the ones in event (QTBUG-57168).
         && !QApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier) )
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->acceptProposedAction();
    }
}

QByteArray makeClipboardOwnerData()
{
    static int id = 0;
    return qgetenv("COPYQ_SESSION_NAME")
           + " " + logLabel()
           + "/" + QByteArray::number(++id);
}

QByteArray clipboardOwnerData(ClipboardMode mode)
{
    const auto data = clipboardData(mode);
    return data ? data->data(mimeOwner) : QByteArray();
}

QString cloneText(const QMimeData &data)
{
    const auto text = dataToText( data.data(mimeText), mimeText );
    return text.isEmpty() ? data.text() : text;
}

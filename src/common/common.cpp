// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/common.h"

#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"

#include <QApplication>
#include <QBuffer>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QImageWriter>
#include <QKeyEvent>
#include <QMimeData>
#include <QMovie>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>
#include <QUrl>

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#   include <QTextCodec>
using Encoding = QTextCodec*;
#else
#   include <QStringDecoder>
using Encoding = std::optional<QStringConverter::Encoding>;
#endif

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
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QVariant retrieveData(const QString &mimeType, QMetaType preferredType) const override
#else
    QVariant retrieveData(const QString &mimeType, QVariant::Type preferredType) const override
#endif
    {
        COPYQ_LOG_VERBOSE( QStringLiteral("Providing \"%1\"").arg(mimeType) );
        return QMimeData::retrieveData(mimeType, preferredType);
    }
};

// Avoids accessing old clipboard/drag'n'drop data.
class ClipboardDataGuard final {
public:
    class ElapsedGuard {
    public:
        explicit ElapsedGuard(const QString &type, const QString &format)
            : m_type(type)
            , m_format(format)
        {
            COPYQ_LOG_VERBOSE( QStringLiteral("Accessing [%1:%2]").arg(type, format) );
            m_elapsed.start();
        }

        ~ElapsedGuard()
        {
            const auto t = m_elapsed.elapsed();
            if (t > 500)
                log( QStringLiteral("ELAPSED %1 ms accessing [%2:%3]").arg(t).arg(m_type, m_format), LogWarning );
        }
    private:
        QString m_type;
        QString m_format;
        QElapsedTimer m_elapsed;
    };

    explicit ClipboardDataGuard(const QMimeData &data, bool *abortCloning = nullptr)
        : m_dataGuard(&data)
        , m_abort(abortCloning)
    {
        m_timerExpire.start();
    }

    QStringList formats()
    {
        ElapsedGuard _(QStringLiteral(), QStringLiteral("formats"));
        return refresh() ? m_dataGuard->formats() : QStringList();
    }

    bool hasFormat(const QString &mime)
    {
        ElapsedGuard _(QStringLiteral("hasFormat"), mime);
        return refresh() && m_dataGuard->hasFormat(mime);
    }

    QByteArray data(const QString &mime)
    {
        ElapsedGuard _(QStringLiteral("data"), mime);
        return refresh() ? m_dataGuard->data(mime) : QByteArray();
    }

    QList<QUrl> urls()
    {
        ElapsedGuard _(QStringLiteral(), QStringLiteral("urls"));
        return refresh() ? m_dataGuard->urls() : QList<QUrl>();
    }

    QString text()
    {
        ElapsedGuard _(QStringLiteral(), QStringLiteral("text"));
        return refresh() ? m_dataGuard->text() : QString();
    }

    bool hasText()
    {
        ElapsedGuard _(QStringLiteral(), QStringLiteral("hasText"));
        return refresh() && m_dataGuard->hasText();
    }

    QImage getImageData()
    {
        ElapsedGuard _(QStringLiteral(), QStringLiteral("imageData"));
        if (!refresh())
            return QImage();

        // NOTE: Application hangs if using multiple sessions and
        //       calling QMimeData::hasImage() on X11 clipboard.
        QImage image = m_dataGuard->imageData().value<QImage>();
        if ( image.isNull() ) {
            image.loadFromData( data(QStringLiteral("image/png")), "png" );
            if ( image.isNull() ) {
                image.loadFromData( data(QStringLiteral("image/bmp")), "bmp" );
            }
        }
        COPYQ_LOG_VERBOSE(
            QStringLiteral("Image is %1")
            .arg(image.isNull() ? QStringLiteral("invalid") : QStringLiteral("valid")) );
        return image;
    }

    QByteArray getUtf8Data(const QString &format)
    {
        ElapsedGuard _(QStringLiteral("UTF8"), format);
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
            log(QStringLiteral("Clipboard data expired, refusing to access old data"), LogWarning);
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
    static const QString imageMimePrefix = QStringLiteral("image/");
    return mime.startsWith(imageMimePrefix) ? mime.mid(imageMimePrefix.length()) : QString();
}

/**
 * Sometimes only Qt internal image data are available in clipboard,
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

    COPYQ_LOG( QStringLiteral("Converting image to \"%1\" format: %2")
               .arg(format,
                    saved ? QStringLiteral("Done") : QStringLiteral("Failed")) );

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

bool setImageData(
    QByteArray &bytes, const char *imageFormat, const std::unique_ptr<QMimeData> &mimeData)
{
    // Omit converting animated images to static ones.
    QBuffer buffer(&bytes);
    const QMovie animatedImage(&buffer, imageFormat);
    if ( animatedImage.frameCount() > 1 )
        return false;

    const QImage image = QImage::fromData(bytes, imageFormat);
    if ( image.isNull() )
        return false;

    mimeData->setImageData(image);
    return true;
}

bool setImageData(
    const QVariantMap &data, const QString &mime, const char *imageFormat, const std::unique_ptr<QMimeData> &mimeData)
{
    QByteArray imageData = data.value(mime).toByteArray();
    return !imageData.isEmpty()
        && setImageData(imageData, imageFormat, mimeData);
}

Encoding encodingForName(const char *name)
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    return QTextCodec::codecForName(name);
#else
    return QStringConverter::encodingForName(name);
#endif
}

Encoding encodingForText(const QByteArray &bytes)
{
    // Guess unicode codec for text if BOM is missing.
    if (bytes.size() >= 2 && bytes.size() % 2 == 0) {
        if (bytes.size() >= 4 && bytes.size() % 4 == 0) {
            if (bytes.at(0) == 0 && bytes.at(1) == 0)
                return encodingForName("utf-32be");
            if (bytes.at(2) == 0 && bytes.at(3) == 0)
                return encodingForName("utf-32le");
        }

        if (bytes.at(0) == 0)
            return encodingForName("utf-16be");

        if (bytes.at(1) == 0)
            return encodingForName("utf-16le");
    }

    return encodingForName("utf-8");
}

bool findFormatsWithPrefix(bool hasPrefix, const QString &prefix, const QVariantMap &data)
{
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        if ( it.key().startsWith(prefix) == hasPrefix )
            return hasPrefix;
    }

    return !hasPrefix;
}

bool isBinaryImageFormat(const QString &format)
{
    return format.startsWith(QStringLiteral("image/"))
           && !format.contains(QStringLiteral("xml"))
           && !format.contains(QStringLiteral("svg"));
}

QVariantMap cloneData(ClipboardDataGuard &data, QStringList &formats)
{
    QVariantMap newdata;

    /*
     Some apps provide images even when copying huge spreadsheet, this can
     block those apps while generating and providing the data.

     This code removes ignores any image data if text is available.

     Images in SVG and other XML formats are expected to be relatively small
     so these doesn't have to be ignored.
     */
    if ( formats.contains(mimeText) && data.hasText() ) {
        const auto first = std::remove_if(
            std::begin(formats), std::end(formats), isBinaryImageFormat);
        formats.erase(first, std::end(formats));
    }

    QStringList imageFormats;
    for (const auto &mime : formats) {
        if (isBinaryImageFormat(mime)) {
            imageFormats.append(mime);
        } else {
            const QByteArray bytes = data.getUtf8Data(mime);
            if ( bytes.isEmpty() )
                imageFormats.append(mime);
            else
                newdata.insert(mime, bytes);
        }
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

    // Drop duplicate UTF-8 text format.
    if ( newdata.contains(mimeTextUtf8) && newdata[mimeTextUtf8] == newdata.value(mimeText) )
        newdata.remove(mimeTextUtf8);

    return newdata;
}

} // namespace

QVariantMap cloneData(const QMimeData &rawData, QStringList formats, bool *abortCloning)
{
    ClipboardDataGuard data(rawData, abortCloning);
    return cloneData(data, formats);
}

QVariantMap cloneData(const QMimeData &rawData)
{
    bool abortCloning = false;
    ClipboardDataGuard data(rawData, &abortCloning);

    static const QSet<QString> ignoredFormats({
        mimeOwner,
        mimeClipboardMode,
        mimeCurrentTab,
        mimeSelectedItems,
        mimeCurrentItem,
        mimeShortcut,
        mimeOutputTab,
    });

    const QStringList availableFormats = data.formats();

    QStringList formats;

    bool skipBinaryImages = formats.contains(mimeText) && data.hasText();
    if (!skipBinaryImages && availableFormats.contains(QStringLiteral("image/png")) ) {
        formats.append(QStringLiteral("image/png"));
        if ( availableFormats.contains(QStringLiteral("image/webp")) )
            formats.append(QStringLiteral("image/webp"));
        if ( availableFormats.contains(QStringLiteral("image/gif")) )
            formats.append(QStringLiteral("image/gif"));
        skipBinaryImages = true;
    }

    for (const auto &mime : availableFormats) {
        // ignore uppercase mimetypes (e.g. UTF8_STRING, TARGETS, TIMESTAMP)
        // and specific internal types
        if ( mime.isEmpty() || !mime[0].isLower() || ignoredFormats.contains(mime) )
            continue;

        // ignore redundant image formats
        if ( skipBinaryImages && isBinaryImageFormat(mime) )
            continue;

        formats.append(mime);
    }

    if ( !formats.contains(mimeText) ) {
        const QString textPrefix(mimeText + QStringLiteral(";"));
        const auto first = std::remove_if(
            std::begin(formats), std::end(formats), [&](const QString &format){
                return format.startsWith(textPrefix);
            }
        );
        if (first != std::end(formats)) {
            formats.erase(first, std::end(formats));
            formats.append(mimeText);
        }
    }

    return cloneData(data, formats);
}

QMimeData* createMimeData(const QVariantMap &data)
{
    std::unique_ptr<QMimeData> newClipboardData(new MimeData);

    bool hasImage =
        setImageData(data, QStringLiteral("image/png"), "png", newClipboardData)
        || setImageData(data, QStringLiteral("image/bmp"), "bmp", newClipboardData)
        || setImageData(data, QStringLiteral("application/x-qt-image"), nullptr, newClipboardData)
    ;

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        if ( it.key() == mimeClipboardMode )
            continue;

        QByteArray bytes = it.value().toByteArray();

        newClipboardData->setData( it.key(), bytes );

        if (!hasImage) {
            const QString imageFormat = getImageFormatFromMime(it.key());
            if ( !imageFormat.isEmpty()
                && setImageData(bytes, imageFormat.toUtf8().constData(), newClipboardData) )
            {
                hasImage = true;
            }
        }
    }

    if ( !data.contains(mimeOwner) ) {
        const auto owner = makeClipboardOwnerData();
        if ( !owner.isEmpty() )
            newClipboardData->setData( mimeOwner, owner );
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
        return QLatin1String("...");

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
        result.replace( QLatin1Char('&'), QLatin1String("&&") );

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
            label = QLatin1String("%1");

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
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto codec = (mime == mimeHtml)
            ? QTextCodec::codecForHtml(bytes, nullptr)
            : QTextCodec::codecForUtfText(bytes, nullptr);

    if (!codec)
        codec = encodingForText(bytes);

    return codec->toUnicode(bytes);
#else
    auto encoding = (mime == mimeHtml)
            ? QStringConverter::encodingForHtml(bytes)
            : QStringConverter::encodingForData(bytes);

    if (!encoding)
        encoding = encodingForText(bytes);

    return QStringDecoder(*encoding).decode(bytes);
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(5,12,0)
         // WORKAROUND: Test currently pressed modifiers instead of the ones in event (QTBUG-57168).
         && !QApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier)
#else
         && !event->keyboardModifiers().testFlag(Qt::ControlModifier)
#endif
        )
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->acceptProposedAction();
    }
}

QByteArray makeClipboardOwnerData()
{
    static const QVariant owner = qApp->property("CopyQ_session_name");
    if ( !owner.isValid() )
        return QByteArray();

    static int id = 0;
    return owner.toString().toUtf8() + " " + logLabel() + "/" + QByteArray::number(++id);
}

QString cloneText(const QMimeData &data)
{
    const auto text = dataToText( data.data(mimeText), mimeText );
    return text.isEmpty() ? data.text() : text;
}

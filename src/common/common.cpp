// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/common.h"

#include "common/clipboarddataguard.h"
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

#include <algorithm>
#include <memory>

namespace {

const int maxElidedTextLineLength = 512;

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

} // namespace

QVariantMap cloneData(ClipboardDataGuard &data, const QStringList &formats)
{
    QVariantMap newdata;

    /*
     Some apps provide images even when copying huge spreadsheet, this can
     block those apps while generating and providing the data.

     This code removes ignores any image data if text is available.

     Images in SVG and other XML formats are expected to be relatively small
     so these doesn't have to be ignored.
     */
    const bool skipBinaryImageFormats = formats.contains(mimeText) && data.hasText();

    QStringList imageFormats;
    for (const auto &mime : formats) {
        if (isBinaryImageFormat(mime)) {
            if (!skipBinaryImageFormats)
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

    if (data.isExpired())
        return {};

    // Drop duplicate UTF-8 text format.
    if ( newdata.contains(mimeTextUtf8) && newdata[mimeTextUtf8] == newdata.value(mimeText) )
        newdata.remove(mimeTextUtf8);

    return newdata;
}

QVariantMap cloneData(const QMimeData *rawData, const QStringList &formats, const long int *clipboardSequenceNumber)
{
    ClipboardDataGuard data(rawData, clipboardSequenceNumber);
    return cloneData(data, formats);
}

QVariantMap cloneData(const QMimeData *rawData)
{
    ClipboardDataGuard data(rawData);

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
    const int formatWidth = format.isEmpty() ? 0 : fm.horizontalAdvance(format.arg(QString()));

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

    if ( data.contains(mimeHidden) || data.contains(mimeSecret) ) {
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
         && !event->keyboardModifiers().testFlag(Qt::ControlModifier)
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

// SPDX-License-Identifier: GPL-3.0-or-later
#include "common/clipboarddataguard.h"
#include "common/common.h"
#include "common/log.h"
#include "common/appconfig.h"
#include "common/mimetypes.h"
#include "common/textdata.h"

#include <QImage>
#include <QList>
#include <QMimeData>
#include <QObject>
#include <QUrl>
#include <QCoreApplication>

#include <QRegularExpression>

#include <limits>
#include <new>

namespace {

int clipboardCopyTimeoutMs()
{
    static bool ok = false;
    static int ms = qEnvironmentVariableIntValue("COPYQ_CLIPBOARD_COPY_TIMEOUT_MS", &ok);
    return ok ? ms : 5000;
}

const QMimeData *dummyMimeData()
{
    static QMimeData mimeData;
    return &mimeData;
}

struct MimeSizeRule {
    QRegularExpression pattern;
    qint64 maxBytes;
};

QRegularExpression anchoredMimePattern(const QString &pattern)
{
    return QRegularExpression(QRegularExpression::anchoredPattern(pattern));
}

qint64 parseByteSize(const QString &str, bool *ok)
{
    const QStringView s = QStringView(str).trimmed();
    if (s.isEmpty()) {
        // Empty size (e.g. "text/plain:") means no limit.
        *ok = true;
        return -1;
    }

    const QChar last = s.last();
    qint64 multiplier = 1;
    QStringView num = s;

    if (last.toUpper() == QLatin1Char('K')) {
        multiplier = 1024LL;
        num = s.chopped(1);
    } else if (last.toUpper() == QLatin1Char('M')) {
        multiplier = 1024LL * 1024;
        num = s.chopped(1);
    } else if (last.toUpper() == QLatin1Char('G')) {
        multiplier = 1024LL * 1024 * 1024;
        num = s.chopped(1);
    }

    const qint64 value = num.toLongLong(ok);
    if (!*ok)
        return 0;
    // Negative means "no limit", consistent with maxBytesForMime() returning -1.
    if (value < 0)
        return value;
    if (multiplier > 1 && value > std::numeric_limits<qint64>::max() / multiplier)
        return -1; // overflow → treat as no limit
    return value * multiplier;
}

QList<MimeSizeRule> mimeSizeRules()
{
    QList<MimeSizeRule> result;

    // Env var takes precedence over config option.
    QString spec = qEnvironmentVariable("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
    if (spec.isEmpty()) {
        AppConfig appConfig;
        spec = appConfig.option<Config::clipboard_mime_size_limit>();
    }

    if (!spec.isEmpty()) {
        const auto entries = spec.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const auto &entry : entries) {
            const int sep = entry.lastIndexOf(QLatin1Char(':'));
            if (sep <= 0)
                continue;
            const QString pattern = entry.left(sep).trimmed();
            bool ok = false;
            const qint64 bytes = parseByteSize(entry.mid(sep + 1), &ok);
            if (!ok)
                continue;
            QRegularExpression re = anchoredMimePattern(pattern);
            if (re.isValid())
                result.append({std::move(re), bytes});
        }
        if (!result.isEmpty())
            return result;
        log(QStringLiteral("clipboard_mime_size_limit: no valid rules in \"%1\", using default (.*:100M)").arg(spec), LogWarning);
    }

    // Built-in default: limit everything to 100 MiB.
    // To disable, set `.*:-1`.
    return {
        {anchoredMimePattern(QStringLiteral(".*")), 100 * 1024 * 1024LL},
    };
}

// Returns -1 when no rule matches (no limit).
qint64 maxBytesForMime(const QString &mime)
{
    static int cachedGeneration = -1;
    static QList<MimeSizeRule> rules;

    const int generation = qApp ? qApp->property("CopyQ_config_generation").toInt() : 0;
    if (generation != cachedGeneration) {
        cachedGeneration = generation;
        rules = mimeSizeRules();
    }

    for (const auto &rule : rules) {
        if (rule.pattern.match(mime).hasMatch())
            return rule.maxBytes;
    }
    return -1;
}

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

} //namespace

ClipboardDataGuard::ClipboardDataGuard(const QMimeData *data, const long int *clipboardSequenceNumber)
    : m_data(data)
    , m_clipboardSequenceNumber(clipboardSequenceNumber)
    , m_clipboardSequenceNumberOriginal(clipboardSequenceNumber ? *clipboardSequenceNumber : 0)
{
    // This uses simple connection to ensure pointer is not destroyed
    // instead of QPointer to work around a possible Qt bug:
    // - https://bugzilla.redhat.com/show_bug.cgi?id=2320093
    // - https://bugzilla.redhat.com/show_bug.cgi?id=2326881
    m_connection = QObject::connect(m_data, &QObject::destroyed, [this](){
        m_data = nullptr;
        log( QByteArrayLiteral("Aborting clipboard cloning: Data deleted"), LogWarning );
    });
    m_timerExpire.start();
}

ClipboardDataGuard::~ClipboardDataGuard()
{
    QObject::disconnect(m_connection);
}

QStringList ClipboardDataGuard::formats()
{
    ElapsedGuard _(QString(), QStringLiteral("formats"));
    return mimeData()->formats();
}

bool ClipboardDataGuard::hasFormat(const QString &mime)
{
    ElapsedGuard _(QStringLiteral("hasFormat"), mime);
    return mimeData()->hasFormat(mime);
}

QByteArray ClipboardDataGuard::data(const QString &mime)
{
    ElapsedGuard _(QStringLiteral("data"), mime);

    const qint64 maxBytes = maxBytesForMime(mime);
    if (maxBytes == 0)
        return {};

    const QMimeData *md = mimeData();
    QByteArray bytes;
    try {
        bytes = md->data(mime);
    } catch (const std::bad_alloc &) {
        log(QStringLiteral("Out of memory reading clipboard data for %1").arg(mime), LogError);
        return {};
    }

    if (maxBytes > 0 && bytes.size() > maxBytes) {
        log( QStringLiteral("Skipping oversized clipboard data for %1: %2 bytes (limit: %3)")
            .arg(mime).arg(bytes.size()).arg(maxBytes), LogNote );
        return {};
    }

    return bytes;
}

QList<QUrl> ClipboardDataGuard::urls()
{
    ElapsedGuard _(QString(), QStringLiteral("urls"));
    const qint64 maxBytes = maxBytesForMime(mimeUriList);
    if (maxBytes == 0)
        return {};
    const QMimeData *md = mimeData();
    try {
        return md->urls();
    } catch (const std::bad_alloc &) {
        log(QStringLiteral("Out of memory reading clipboard URLs"), LogError);
        return {};
    }
}

QString ClipboardDataGuard::text()
{
    ElapsedGuard _(QString(), QStringLiteral("text"));
    const qint64 maxBytes = maxBytesForMime(mimeText);
    if (maxBytes == 0)
        return {};
    const QMimeData *md = mimeData();
    try {
        return md->text();
    } catch (const std::bad_alloc &) {
        log(QStringLiteral("Out of memory reading clipboard text"), LogError);
        return {};
    }
}

bool ClipboardDataGuard::hasText()
{
    ElapsedGuard _(QString(), QStringLiteral("hasText"));
    return mimeData()->hasText();
}

QImage ClipboardDataGuard::getImageData()
{
    ElapsedGuard _(QString(), QStringLiteral("imageData"));

    // Use "image/" as probe: matches broad rules like image/.*:0
    // but not format-specific ones like image/png:0.
    const qint64 maxBytes = maxBytesForMime(QStringLiteral("image/"));
    if (maxBytes == 0)
        return {};

    const QMimeData *md = mimeData();
    QImage image;

    try {
        // NOTE: Application hangs if using multiple sessions and
        //       calling QMimeData::hasImage() on X11 clipboard.
        image = md->imageData().value<QImage>();
    } catch (const std::bad_alloc &) {
        log(QStringLiteral("Out of memory reading clipboard image data"), LogError);
        return {};
    }

    // Fall back to serialized image formats.
    // data() enforces per-MIME size limits for image/png and image/bmp.
    if ( image.isNull() ) {
        image.loadFromData( data(QStringLiteral("image/png")), "png" );
        if ( image.isNull() ) {
            image.loadFromData( data(QStringLiteral("image/bmp")), "bmp" );
        }
    }

    if (maxBytes > 0 && !image.isNull() && image.sizeInBytes() > maxBytes) {
        log( QStringLiteral("Skipping oversized image data: %1 bytes (limit: %2)")
            .arg(image.sizeInBytes()).arg(maxBytes), LogNote );
        return {};
    }

    COPYQ_LOG_VERBOSE(
        QStringLiteral("Image is %1")
        .arg(image.isNull() ? QStringLiteral("invalid") : QStringLiteral("valid")) );
    return image;
}

QByteArray ClipboardDataGuard::getUtf8Data(const QString &format)
{
    ElapsedGuard _(QStringLiteral("UTF8"), format);

    const qint64 maxBytes = maxBytesForMime(format);
    if (maxBytes == 0)
        return {};

    QByteArray bytes;

    try {
        if (format == mimeUriList) {
            for ( const auto &url : urls() ) {
                if ( !bytes.isEmpty() )
                    bytes += '\n';
                bytes += url.toString().toUtf8();
            }
        } else if ( format == mimeText && !hasFormat(mimeText) ) {
            bytes = text().toUtf8();
        } else if ( format.startsWith(QLatin1String("text/")) ) {
            bytes = dataToText( data(format), format ).toUtf8();
        } else {
            bytes = data(format);
        }
    } catch (const std::bad_alloc &) {
        log(QStringLiteral("Out of memory reading clipboard data for %1").arg(format), LogError);
        return {};
    }

    if (maxBytes > 0 && bytes.size() > maxBytes) {
        log( QStringLiteral("Skipping oversized clipboard data for %1: %2 bytes (limit: %3)")
            .arg(format).arg(bytes.size()).arg(maxBytes), LogNote );
        return {};
    }

    return bytes;
}

bool ClipboardDataGuard::isExpired() {
    if (!m_data)
        return true;

    if (qApp && qApp->property("CopyQ_quitting").toBool()) {
        m_data = nullptr;
        log( QByteArrayLiteral("Aborting clipboard cloning: Application quitting"), LogWarning );
        return true;
    }

    if (m_clipboardSequenceNumber && *m_clipboardSequenceNumber != m_clipboardSequenceNumberOriginal) {
        m_data = nullptr;
        log( QByteArrayLiteral("Aborting clipboard cloning: Clipboard changed again"), LogWarning );
        return true;
    }

    if (m_timerExpire.elapsed() > clipboardCopyTimeoutMs()) {
        m_data = nullptr;
        log( QByteArrayLiteral("Aborting clipboard cloning: Data access took too long"), LogWarning );
        return true;
    }

    return false;
}

const QMimeData *ClipboardDataGuard::mimeData()
{
    if (isExpired())
        return dummyMimeData();

    if (m_timerExpire.elapsed() > 100) {
        QCoreApplication::processEvents();
        if (isExpired())
            return dummyMimeData();
    }

    return m_data;
}

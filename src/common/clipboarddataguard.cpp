// SPDX-License-Identifier: GPL-3.0-or-later
#include "common/clipboarddataguard.h"
#include "common/common.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"

#include <QImage>
#include <QList>
#include <QMimeData>
#include <QObject>
#include <QUrl>
#include <QCoreApplication>

namespace {

int clipboardCopyTimeoutMs()
{
    static bool ok = false;
    static int ms = qgetenv("COPYQ_CLIPBOARD_COPY_TIMEOUT_MS").toInt(&ok);
    return ok ? ms : 5000;
}

const QMimeData *dummyMimeData()
{
    static QMimeData mimeData;
    return &mimeData;
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
    return mimeData()->data(mime);
}

QList<QUrl> ClipboardDataGuard::urls()
{
    ElapsedGuard _(QString(), QStringLiteral("urls"));
    return mimeData()->urls();
}

QString ClipboardDataGuard::text()
{
    ElapsedGuard _(QString(), QStringLiteral("text"));
    return mimeData()->text();
}

bool ClipboardDataGuard::hasText()
{
    ElapsedGuard _(QString(), QStringLiteral("hasText"));
    return mimeData()->hasText();
}

QImage ClipboardDataGuard::getImageData()
{
    ElapsedGuard _(QString(), QStringLiteral("imageData"));

    // NOTE: Application hangs if using multiple sessions and
    //       calling QMimeData::hasImage() on X11 clipboard.
    QImage image = mimeData()->imageData().value<QImage>();
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

QByteArray ClipboardDataGuard::getUtf8Data(const QString &format)
{
    ElapsedGuard _(QStringLiteral("UTF8"), format);

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

    if ( format.startsWith(QLatin1String("text/")) )
        return dataToText( data(format), format ).toUtf8();

    return data(format);
}

bool ClipboardDataGuard::isExpired() {
    if (!m_data)
        return true;

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

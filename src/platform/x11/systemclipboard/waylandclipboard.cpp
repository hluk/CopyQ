/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "waylandclipboard.h"

#include <QBuffer>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QMimeData>
#include <QThread>
#include <QtWaylandClient/QWaylandClientExtension>
#include <qpa/qplatformnativeinterface.h>
#include <qtwaylandclientversion.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "qwayland-wlr-data-control-unstable-v1.h"

static inline QString applicationQtXImageLiteral()
{
    return QStringLiteral("application/x-qt-image");
}

// copied from https://code.woboq.org/qt5/qtbase/src/gui/kernel/qinternalmimedata.cpp.html
static QString utf8Text()
{
    return QStringLiteral("text/plain;charset=utf-8");
}

static QStringList imageMimeFormats(const QList<QByteArray> &imageFormats)
{
    QStringList formats;
    formats.reserve(imageFormats.size());
    for (const auto &format : imageFormats)
        formats.append(QLatin1String("image/") + QLatin1String(format.toLower()));
    // put png at the front because it is best
    int pngIndex = formats.indexOf(QLatin1String("image/png"));
    if (pngIndex != -1 && pngIndex != 0)
        formats.move(pngIndex, 0);
    return formats;
}

static inline QStringList imageReadMimeFormats()
{
    return imageMimeFormats(QImageReader::supportedImageFormats());
}

static inline QStringList imageWriteMimeFormats()
{
    return imageMimeFormats(QImageWriter::supportedImageFormats());
}
// end copied

namespace {

class SendThread : public QThread {
public:
    SendThread(int fd, const QByteArray &data)
        : m_data(data)
        , m_fd(fd)
    {}

protected:
    void run() override {
        QFile c;
        if (c.open(m_fd, QFile::WriteOnly, QFile::AutoCloseHandle)) {
            // Create a sigpipe handler that does nothing, or clients may be forced to terminate
            // if the pipe is closed in the other end.
            struct sigaction action, oldAction;
            action.sa_handler = SIG_IGN;
            sigemptyset(&action.sa_mask);
            action.sa_flags = 0;
            sigaction(SIGPIPE, &action, &oldAction);
            // Unset O_NONBLOCK
            fcntl(m_fd, F_SETFL, 0);
            const qint64 written = c.write(m_data);
            sigaction(SIGPIPE, &oldAction, nullptr);
            c.close();

            if (written != m_data.size()) {
                qWarning() << "Failed to send all clipobard data; sent"
                           << written << "bytes out of" << m_data.size();
            }
        }
    }

private:
    QByteArray m_data;
    int m_fd;
};

class ReceiveThread : public QThread {
public:
    ReceiveThread(int fd)
        : m_fd(fd)
    {}

    QByteArray data() const { return m_data; }

protected:
    void run() override {
        QFile readPipe;
        if (readPipe.open(m_fd, QIODevice::ReadOnly, QFile::AutoCloseHandle)) {
            readData();
            close(m_fd);
        }
    }

private:
    void readData() {
        pollfd pfds[1];
        pfds[0].fd = m_fd;
        pfds[0].events = POLLIN;

        while (true) {
            const int ready = poll(pfds, 1, 1000);
            if (ready < 0) {
                if (errno != EINTR) {
                    qWarning("DataControlOffer: poll() failed: %s", strerror(errno));
                    return;
                }
            } else if (ready == 0) {
                qWarning("DataControlOffer: timeout reading from pipe");
                return;
            } else {
                char buf[4096];
                int n = read(m_fd, buf, sizeof buf);

                if (n < 0) {
                    qWarning("DataControlOffer: read() failed: %s", strerror(errno));
                    return;
                } else if (n == 0) {
                    return;
                } else if (n > 0) {
                    m_data.append(buf, n);
                }
            }
        }
    }

    QByteArray m_data;
    int m_fd;
};

} // namespace

class DataControlDeviceManager : public QWaylandClientExtensionTemplate<DataControlDeviceManager>
        , public QtWayland::zwlr_data_control_manager_v1
{
    Q_OBJECT
public:
    DataControlDeviceManager()
        : QWaylandClientExtensionTemplate<DataControlDeviceManager>(2)
    {
    }

    void instantiate()
    {
#if QTWAYLANDCLIENT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
        initialize();
#else
        // QWaylandClientExtensionTemplate invokes this with a QueuedConnection but we want shortcuts
        // to be have access to data_control immediately.
        QMetaObject::invokeMethod(this, "addRegistryListener");
#endif
    }

    ~DataControlDeviceManager()
    {
        if (isInitialized()) {
            destroy();
        }
    }
};

class DataControlOffer : public QMimeData, public QtWayland::zwlr_data_control_offer_v1
{
    Q_OBJECT
public:
    DataControlOffer(struct ::zwlr_data_control_offer_v1 *id)
        : QtWayland::zwlr_data_control_offer_v1(id)
    {
    }

    ~DataControlOffer()
    {
        if ( isInitialized() )
            destroy();
    }

    QStringList formats() const override
    {
        return m_receivedFormats;
    }

    bool containsImageData() const
    {
        if (m_receivedFormats.contains(applicationQtXImageLiteral())) {
            return true;
        }
        const auto formats = imageReadMimeFormats();
        for (const auto &receivedFormat : m_receivedFormats) {
            if (formats.contains(receivedFormat)) {
                return true;
            }
        }
        return false;
    }

    bool hasFormat(const QString &mimeType) const override
    {
        if (mimeType == QStringLiteral("text/plain") && m_receivedFormats.contains(utf8Text())) {
            return true;
        }
        if (m_receivedFormats.contains(mimeType)) {
            return true;
        }

        // If we have image data
        if (containsImageData()) {
            // is the requested output mimeType supported ?
            const QStringList imageFormats = imageWriteMimeFormats();
            for (const QString &imageFormat : imageFormats) {
                if (imageFormat == mimeType) {
                    return true;
                }
            }
            if (mimeType == applicationQtXImageLiteral()) {
                return true;
            }
        }

        return false;
    }

protected:
    void zwlr_data_control_offer_v1_offer(const QString &mime_type) override
    {
        m_receivedFormats << mime_type;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVariant retrieveData(const QString &mimeType, QMetaType type) const override;
#else
    QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;
#endif

private:
    QStringList m_receivedFormats;
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QVariant DataControlOffer::retrieveData(const QString &mimeType, QMetaType type) const
#else
QVariant DataControlOffer::retrieveData(const QString &mimeType, QVariant::Type type) const
#endif
{
    Q_UNUSED(type);

    QString mime;
    if (!m_receivedFormats.contains(mimeType)) {
        if (mimeType == QStringLiteral("text/plain") && m_receivedFormats.contains(utf8Text())) {
            mime = utf8Text();
        } else if (mimeType == applicationQtXImageLiteral()) {
            const auto writeFormats = imageWriteMimeFormats();
            for (const auto &receivedFormat : m_receivedFormats) {
                if (writeFormats.contains(receivedFormat)) {
                    mime = receivedFormat;
                    break;
                }
            }
            if (mime.isEmpty()) {
                // default exchange format
                mime = QStringLiteral("image/png");
            }
        }

        if (mime.isEmpty()) {
            return QVariant();
        }
    } else {
        mime = mimeType;
    }

    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        return QVariant();
    }

    auto t = const_cast<DataControlOffer *>(this);
    t->receive(mime, pipeFds[1]);

    close(pipeFds[1]);

    /*
     * Ideally we need to introduce a non-blocking QMimeData object
     * Or a non-blocking constructor to QMimeData with the mimetypes that are relevant
     *
     * However this isn't actually any worse than X.
     */

    QPlatformNativeInterface *native = qGuiApp->platformNativeInterface();
    auto display = static_cast<struct ::wl_display *>(native->nativeResourceForIntegration("wl_display"));
    wl_display_flush(display);

    ReceiveThread thread(pipeFds[0]);
    thread.start();
    while (thread.isRunning())
        QCoreApplication::processEvents();
    const auto data = thread.data();

    if (!data.isEmpty() && mimeType == applicationQtXImageLiteral()) {
        QImage img = QImage::fromData(data, mime.mid(mime.indexOf(QLatin1Char('/')) + 1).toLatin1().toUpper().data());
        if (!img.isNull()) {
            return img;
        }
    }

    return data;
}

class DataControlSource : public QObject, public QtWayland::zwlr_data_control_source_v1
{
    Q_OBJECT
public:
    DataControlSource(struct ::zwlr_data_control_source_v1 *id, QMimeData *mimeData);

    ~DataControlSource()
    {
        if (m_mimeData) {
            m_mimeData->deleteLater();
            m_mimeData = nullptr;
        }
        if ( isInitialized() )
            destroy();
    }

    QMimeData *mimeData()
    {
        return m_mimeData;
    }

    bool isCancelled() const { return m_cancelled; }

Q_SIGNALS:
    void cancelled();

protected:
    void zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd) override;
    void zwlr_data_control_source_v1_cancelled() override;

private:
    QMimeData *m_mimeData = nullptr;
    bool m_cancelled = false;
};

DataControlSource::DataControlSource(struct ::zwlr_data_control_source_v1 *id, QMimeData *mimeData)
    : QtWayland::zwlr_data_control_source_v1(id)
    , m_mimeData(mimeData)
{
    const auto formats = mimeData->formats();
    for (const QString &format : formats) {
        offer(format);
    }
    if (mimeData->hasText()) {
        // ensure GTK applications get this mimetype to avoid them discarding the offer
        offer(QStringLiteral("text/plain;charset=utf-8"));
    }

    if (mimeData->hasImage()) {
        const QStringList imageFormats = imageWriteMimeFormats();
        for (const QString &imageFormat : imageFormats) {
            if (!formats.contains(imageFormat)) {
                offer(imageFormat);
            }
        }
    }
}

void DataControlSource::zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd)
{
    QString send_mime_type = mime_type;
    if( send_mime_type == QStringLiteral("text/plain;charset=utf-8")
        && !m_mimeData->hasFormat(QStringLiteral("text/plain;charset=utf-8")) )
    {
        // if we get a request on the fallback mime, send the data from the original mime type
        send_mime_type = QStringLiteral("text/plain");
    }

    QByteArray ba;
    if (m_mimeData->hasImage()) {
        // adapted from QInternalMimeData::renderDataHelper
        if (mime_type == applicationQtXImageLiteral()) {
            QImage image = qvariant_cast<QImage>(m_mimeData->imageData());
            QBuffer buf(&ba);
            buf.open(QBuffer::WriteOnly);
            // would there not be PNG ??
            image.save(&buf, "PNG");

        } else if (mime_type.startsWith(QLatin1String("image/"))) {
            QImage image = qvariant_cast<QImage>(m_mimeData->imageData());
            QBuffer buf(&ba);
            buf.open(QBuffer::WriteOnly);
            image.save(&buf, mime_type.mid(mime_type.indexOf(QLatin1Char('/')) + 1).toLatin1().toUpper().data());
        }
        // end adapted
    } else {
        ba = m_mimeData->data(send_mime_type);
    }

    auto thread = new SendThread(fd, m_mimeData->data(send_mime_type));
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void DataControlSource::zwlr_data_control_source_v1_cancelled()
{
    m_cancelled = true;
    Q_EMIT cancelled();
}

class DataControlDevice : public QObject, public QtWayland::zwlr_data_control_device_v1
{
    Q_OBJECT
public:
    DataControlDevice(struct ::zwlr_data_control_device_v1 *id)
        : QtWayland::zwlr_data_control_device_v1(id)
    {
    }

    ~DataControlDevice()
    {
        if ( isInitialized() )
            destroy();
    }

    void setSelection(std::unique_ptr<DataControlSource> selection);
    QMimeData *receivedSelection()
    {
        return m_receivedSelection.get();
    }
    QMimeData *selection()
    {
        return m_selection ? m_selection->mimeData() : nullptr;
    }

    void setPrimarySelection(std::unique_ptr<DataControlSource> selection);
    QMimeData *receivedPrimarySelection()
    {
        return m_receivedPrimarySelection.get();
    }
    QMimeData *primarySelection()
    {
        return m_primarySelection ? m_primarySelection->mimeData() : nullptr;
    }

Q_SIGNALS:
    void receivedSelectionChanged();
    void selectionChanged();

    void receivedPrimarySelectionChanged();
    void primarySelectionChanged();

protected:
    void zwlr_data_control_device_v1_data_offer(struct ::zwlr_data_control_offer_v1 *id) override
    {
        // this will become memory managed when we retrieve the selection event
        // a compositor calling data_offer without doing that would be a bug
        new DataControlOffer(id);
    }

    void zwlr_data_control_device_v1_selection(struct ::zwlr_data_control_offer_v1 *id) override
    {
        if (!id) {
            m_receivedSelection.reset();
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5,12,5)
            auto derivated = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
#else
            auto derivated = static_cast<QtWayland::zwlr_data_control_offer_v1 *>(zwlr_data_control_offer_v1_get_user_data(id));
#endif
            auto offer = dynamic_cast<DataControlOffer *>(derivated); // dynamic because of the dual inheritance
            m_receivedSelection.reset(offer);
        }
        Q_EMIT receivedSelectionChanged();
    }

    void zwlr_data_control_device_v1_primary_selection(struct ::zwlr_data_control_offer_v1 *id) override
    {
        if (!id) {
            m_receivedPrimarySelection.reset();
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5,12,5)
            auto derivated = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
#else
            auto derivated = static_cast<QtWayland::zwlr_data_control_offer_v1 *>(zwlr_data_control_offer_v1_get_user_data(id));
#endif
            auto offer = dynamic_cast<DataControlOffer *>(derivated); // dynamic because of the dual inheritance
            m_receivedPrimarySelection.reset(offer);
        }
        Q_EMIT receivedPrimarySelectionChanged();
    }

private:
    std::unique_ptr<DataControlSource> m_selection; // selection set locally
    std::unique_ptr<DataControlOffer> m_receivedSelection; // latest selection set from externally to here

    std::unique_ptr<DataControlSource> m_primarySelection; // selection set locally
    std::unique_ptr<DataControlOffer> m_receivedPrimarySelection; // latest selection set from externally to here
};

void DataControlDevice::setSelection(std::unique_ptr<DataControlSource> selection)
{
    m_selection = std::move(selection);
    connect(m_selection.get(), &DataControlSource::cancelled, this, [this]() {
        m_selection.reset();
        Q_EMIT selectionChanged();
    });
    set_selection(m_selection->object());
    Q_EMIT selectionChanged();
}

void DataControlDevice::setPrimarySelection(std::unique_ptr<DataControlSource> selection)
{
    m_primarySelection = std::move(selection);
    connect(m_primarySelection.get(), &DataControlSource::cancelled, this, [this]() {
        m_primarySelection.reset();
        Q_EMIT primarySelectionChanged();
    });

    if (zwlr_data_control_device_v1_get_version(object()) >= ZWLR_DATA_CONTROL_DEVICE_V1_SET_PRIMARY_SELECTION_SINCE_VERSION) {
        set_primary_selection(m_primarySelection->object());
        Q_EMIT primarySelectionChanged();
    }
}

WaylandClipboard::WaylandClipboard(QObject *parent)
    : QObject(parent)
    , m_manager(new DataControlDeviceManager)
{
    connect(m_manager.get(), &DataControlDeviceManager::activeChanged, this, [this]() {
        if (m_manager->isActive()) {
            QPlatformNativeInterface *native = qApp->platformNativeInterface();
            if (!native) {
                return;
            }
            auto seat = static_cast<struct ::wl_seat *>(native->nativeResourceForIntegration("wl_seat"));
            if (!seat) {
                return;
            }

            m_device.reset(new DataControlDevice(m_manager->get_data_device(seat)));

            connect(m_device.get(), &DataControlDevice::receivedSelectionChanged, this, [this]() {
                // When our source is still valid, so the offer is for setting it or we emit changed when it is cancelled
                if (!m_device->selection()) {
                    Q_EMIT changed(QClipboard::Clipboard);
                }
            });
            connect(m_device.get(), &DataControlDevice::selectionChanged, this, [this]() {
                Q_EMIT changed(QClipboard::Clipboard);
            });

            connect(m_device.get(), &DataControlDevice::receivedPrimarySelectionChanged, this, [this]() {
                // When our source is still valid, so the offer is for setting it or we emit changed when it is cancelled
                if (!m_device->primarySelection()) {
                    Q_EMIT changed(QClipboard::Selection);
                }
            });
            connect(m_device.get(), &DataControlDevice::primarySelectionChanged, this, [this]() {
                Q_EMIT changed(QClipboard::Selection);
            });

        } else {
            m_device.reset();
        }
    });

    m_manager->instantiate();
}

WaylandClipboard *WaylandClipboard::createInstance()
{
    qInfo() << "Using Wayland clipboard access";
    auto clipboard = new WaylandClipboard(qApp);

    QElapsedTimer timer;
    timer.start();
    while ( !clipboard->isActive() && timer.elapsed() < 5000 ) {
        QCoreApplication::processEvents();
    }
    if ( timer.elapsed() > 100 ) {
        qWarning() << "Activating Wayland clipboard took" << timer.elapsed() << "ms";
    }
    if ( !clipboard->isActive() ) {
        qCritical() << "Failed to activate Wayland clipboard";
    }
    return clipboard;
}

void WaylandClipboard::setMimeData(QMimeData *mime, QClipboard::Mode mode)
{
    if (!m_device) {
        return;
    }
    std::unique_ptr<DataControlSource> source(new DataControlSource(m_manager->create_data_source(), mime));
    if (mode == QClipboard::Clipboard) {
        m_device->setSelection(std::move(source));
    } else if (mode == QClipboard::Selection) {
        m_device->setPrimarySelection(std::move(source));
    }
}

void WaylandClipboard::clear(QClipboard::Mode mode)
{
    if (!m_device) {
        return;
    }
    if (mode == QClipboard::Clipboard) {
        m_device->set_selection(nullptr);
    } else if (mode == QClipboard::Selection) {
        if (zwlr_data_control_device_v1_get_version(m_device->object()) >= ZWLR_DATA_CONTROL_DEVICE_V1_SET_PRIMARY_SELECTION_SINCE_VERSION) {
            m_device->set_primary_selection(nullptr);
        }
    }
}

const QMimeData *WaylandClipboard::mimeData(QClipboard::Mode mode) const
{
    if (!m_device) {
        return nullptr;
    }

    // return our locally set selection if it's not cancelled to avoid copying data to ourselves
    if (mode == QClipboard::Clipboard) {
        if (m_device->selection()) {
            return m_device->selection();
        }
        return m_device->receivedSelection();
    } else if (mode == QClipboard::Selection) {
        if (m_device->primarySelection()) {
            return m_device->primarySelection();
        }
        return m_device->receivedPrimarySelection();
    }
    return nullptr;
}

bool WaylandClipboard::isSelectionSupported() const
{
    return m_device && zwlr_data_control_device_v1_get_version(m_device->object())
            >= ZWLR_DATA_CONTROL_DEVICE_V1_SET_PRIMARY_SELECTION_SINCE_VERSION;
}

WaylandClipboard *WaylandClipboard::instance()
{
    static WaylandClipboard *clipboard = createInstance();
    return clipboard;
}

WaylandClipboard::~WaylandClipboard() = default;

#include "waylandclipboard.moc"

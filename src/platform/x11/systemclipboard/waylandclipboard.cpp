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

#include "qwayland-wayland.h"
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

} // namespace

class Keyboard;
// We are binding to Seat/Keyboard manually because we want to react to gaining focus but inside Qt the events are Qt and arrive to late
class KeyboardFocusWatcher : public QWaylandClientExtensionTemplate<KeyboardFocusWatcher>, public QtWayland::wl_seat
{
    Q_OBJECT
public:
    KeyboardFocusWatcher()
        : QWaylandClientExtensionTemplate(5)
    {
#if QTWAYLANDCLIENT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
        initialize();
#else
        // QWaylandClientExtensionTemplate invokes this with a QueuedConnection but we want shortcuts
        // to be have access to data_control immediately.
        QMetaObject::invokeMethod(this, "addRegistryListener");
#endif
        QPlatformNativeInterface *native = qGuiApp->platformNativeInterface();
        auto display = static_cast<struct ::wl_display *>(native->nativeResourceForIntegration("wl_display"));
        // so we get capabilities
        wl_display_roundtrip(display);
    }
    ~KeyboardFocusWatcher() override
    {
        if (isActive()) {
            release();
        }
    }
    void seat_capabilities(uint32_t capabilities) override
    {
        const bool hasKeyboard = capabilities & capability_keyboard;
        if (hasKeyboard && !m_keyboard) {
            m_keyboard = std::make_unique<Keyboard>(get_keyboard(), *this);
        } else if (!hasKeyboard && m_keyboard) {
            m_keyboard.reset();
        }
    }
    bool hasFocus() const
    {
        return m_focus;
    }
Q_SIGNALS:
    void keyboardEntered();

private:
    friend Keyboard;
    bool m_focus = false;
    std::unique_ptr<Keyboard> m_keyboard;
};

class Keyboard : public QtWayland::wl_keyboard
{
public:
    Keyboard(::wl_keyboard *keyboard, KeyboardFocusWatcher &seat)
        : wl_keyboard(keyboard)
        , m_seat(seat)
    {
    }
    ~Keyboard()
    {
        release();
    }

private:
    void keyboard_enter([[maybe_unused]] uint32_t serial, [[maybe_unused]] wl_surface *surface, [[maybe_unused]] wl_array *keys) override
    {
        m_seat.m_focus = true;
        Q_EMIT m_seat.keyboardEntered();
    }
    void keyboard_leave([[maybe_unused]] uint32_t serial, [[maybe_unused]] wl_surface *surface) override
    {
        m_seat.m_focus = false;
    }
    KeyboardFocusWatcher &m_seat;
};

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
    QClipboard::Mode clipboardMode = QClipboard::Clipboard;

    DataControlOffer(struct ::zwlr_data_control_offer_v1 *id, const std::shared_ptr<KeyboardFocusWatcher> &keyboardFocusWatcher)
        : QtWayland::zwlr_data_control_offer_v1(id)
        , m_keyboardFocusWatcher(keyboardFocusWatcher)
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
        if (!m_receivedFormats.contains(mime_type)) {
            m_receivedFormats << mime_type;
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVariant retrieveData(const QString &mimeType, QMetaType type) const override;
#else
    QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;
#endif

private:
    /** reads data from a file descriptor with a timeout of 1 second
     *  true if data is read successfully
     */
    static bool readData(int fd, QByteArray &data);
    QStringList m_receivedFormats;
    mutable QHash<QString, QVariant> m_data;
    std::shared_ptr<KeyboardFocusWatcher> m_keyboardFocusWatcher;
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QVariant DataControlOffer::retrieveData(const QString &mimeType, QMetaType type) const
#else
QVariant DataControlOffer::retrieveData(const QString &mimeType, QVariant::Type type) const
#endif
{
    Q_UNUSED(type);

    if (m_keyboardFocusWatcher->hasFocus()) {
        const auto mimeData = QGuiApplication::clipboard()->mimeData(clipboardMode);
        if (mimeData)
            return mimeData->data(mimeType);
        return {};
    }

    auto it = m_data.constFind(mimeType);
    if (it != m_data.constEnd())
        return *it;

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

    QFile readPipe;
    if (readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
        QByteArray data;
        if (readData(pipeFds[0], data)) {
            close(pipeFds[0]);

            if (mimeType == applicationQtXImageLiteral()) {
                QImage img = QImage::fromData(data, mime.mid(mime.indexOf(QLatin1Char('/')) + 1).toLatin1().toUpper().data());
                if (!img.isNull()) {
                    m_data.insert(mimeType, img);
                    return img;
                }
            } else if (data.size() > 1 && mimeType == u"text/uri-list") {
                const auto urls = data.split('\n');
                QVariantList list;
                list.reserve(urls.size());
                for (const QByteArray &s : urls) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                    if (QUrl url(QUrl::fromEncoded(QByteArrayView(s).trimmed())); url.isValid()) {
                        list.emplace_back(std::move(url));
                    }
#else
                    if (QUrl url(QUrl::fromEncoded(s.trimmed())); url.isValid()) {
                        list.push_back(std::move(url));
                    }
#endif
                }
                m_data.insert(mimeType, list);
                return list;
            }
            m_data.insert(mimeType, data);
            return data;
        }
        close(pipeFds[0]);
    }

    return QVariant();
}

bool DataControlOffer::readData(int fd, QByteArray &data)
{
    pollfd pfds[1];
    pfds[0].fd = fd;
    pfds[0].events = POLLIN;

    while (true) {
        const int ready = poll(pfds, 1, 1000);
        if (ready < 0) {
            if (errno != EINTR) {
                qWarning("DataControlOffer: poll() failed: %s", strerror(errno));
                return false;
            }
        } else if (ready == 0) {
            qWarning("DataControlOffer: timeout reading from pipe");
            return false;
        } else {
            char buf[4096];
            int n = read(fd, buf, sizeof buf);

            if (n < 0) {
                qWarning("DataControlOffer: read() failed: %s", strerror(errno));
                return false;
            } else if (n == 0) {
                return true;
            } else if (n > 0) {
                data.append(buf, n);
            }
        }
    }
}

class DataControlSource : public QObject, public QtWayland::zwlr_data_control_source_v1
{
    Q_OBJECT
public:
    DataControlSource(struct ::zwlr_data_control_source_v1 *id, QMimeData *mimeData);

    ~DataControlSource()
    {
        if ( isInitialized() )
            destroy();
    }

    QMimeData *mimeData()
    {
        return m_mimeData.get();
    }
    std::unique_ptr<QMimeData> releaseMimeData()
    {
        return std::move(m_mimeData);
    }

    bool isCancelled() const { return m_cancelled; }

Q_SIGNALS:
    void cancelled();

protected:
    void zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd) override;
    void zwlr_data_control_source_v1_cancelled() override;

private:
    std::unique_ptr<QMimeData> m_mimeData;
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
    DataControlDevice(struct ::zwlr_data_control_device_v1 *id, const std::shared_ptr<KeyboardFocusWatcher> &keyboardFocusWatcher)
        : QtWayland::zwlr_data_control_device_v1(id)
        , m_keyboardFocusWatcher(keyboardFocusWatcher)
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
        new DataControlOffer(id, m_keyboardFocusWatcher);
    }

    void zwlr_data_control_device_v1_selection(struct ::zwlr_data_control_offer_v1 *id) override
    {
        if (!id) {
            m_receivedSelection.reset();
        } else {
            auto derivated = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
            auto offer = dynamic_cast<DataControlOffer *>(derivated); // dynamic because of the dual inheritance
            if (offer)
                offer->clipboardMode = QClipboard::Clipboard;
            m_receivedSelection.reset(offer);
        }
        Q_EMIT receivedSelectionChanged();
    }

    void zwlr_data_control_device_v1_primary_selection(struct ::zwlr_data_control_offer_v1 *id) override
    {
        if (!id) {
            m_receivedPrimarySelection.reset();
        } else {
            auto derivated = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
            auto offer = dynamic_cast<DataControlOffer *>(derivated); // dynamic because of the dual inheritance
            if (offer)
                offer->clipboardMode = QClipboard::Selection;
            m_receivedPrimarySelection.reset(offer);
        }
        Q_EMIT receivedPrimarySelectionChanged();
    }

private:
    std::shared_ptr<KeyboardFocusWatcher> m_keyboardFocusWatcher;

    std::unique_ptr<DataControlSource> m_selection; // selection set locally
    std::unique_ptr<DataControlOffer> m_receivedSelection; // latest selection set from externally to here

    std::unique_ptr<DataControlSource> m_primarySelection; // selection set locally
    std::unique_ptr<DataControlOffer> m_receivedPrimarySelection; // latest selection set from externally to here
    friend WaylandClipboard;
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
    , m_keyboardFocusWatcher(new KeyboardFocusWatcher)
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

            m_device.reset(new DataControlDevice(m_manager->get_data_device(seat), m_keyboardFocusWatcher));

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
    m_deviceRequestedTimer.start();
}

WaylandClipboard *WaylandClipboard::createInstance()
{
    return new WaylandClipboard(qApp);
}

void WaylandClipboard::setMimeData(QMimeData *mime, QClipboard::Mode mode)
{
    if (!waitForDevice(1000)) {
        return;
    }

    // roundtrip to have accurate focus state when losing focus but setting mime data before processing wayland events.
    QPlatformNativeInterface *native = qGuiApp->platformNativeInterface();
    auto display = static_cast<struct ::wl_display *>(native->nativeResourceForIntegration("wl_display"));
    wl_display_roundtrip(display);

    // If the application is focused, use the normal mechanism so a future paste will not deadlock itself
    if (m_keyboardFocusWatcher->hasFocus()) {
        QGuiApplication::clipboard()->setMimeData(mime, mode);
        // if we short-circuit the wlr_data_device, when we receive the data
        // we cannot identify ourselves as the owner
        // because of that we act like it's a synchronous action to not confuse klipper.
        wl_display_roundtrip(display);
        return;
    }
    // If not, set the clipboard once the app receives focus to avoid the deadlock
    connect(m_keyboardFocusWatcher.get(), &KeyboardFocusWatcher::keyboardEntered, this, &WaylandClipboard::gainedFocus, Qt::UniqueConnection);
    std::unique_ptr<DataControlSource> source(new DataControlSource(m_manager->create_data_source(), mime));
    if (mode == QClipboard::Clipboard) {
        m_device->setSelection(std::move(source));
    } else if (mode == QClipboard::Selection) {
        m_device->setPrimarySelection(std::move(source));
    }
}

void WaylandClipboard::gainedFocus()
{
    disconnect(m_keyboardFocusWatcher.get(), &KeyboardFocusWatcher::keyboardEntered, this, nullptr);
    // QClipboard takes ownership of the QMimeData so we need to transfer and unset our selections
    if (auto &selection = m_device->m_selection) {
        std::unique_ptr<QMimeData> data = selection->releaseMimeData();
        selection.reset();
        QGuiApplication::clipboard()->setMimeData(data.release(), QClipboard::Clipboard);
    }
    if (auto &primarySelection = m_device->m_primarySelection) {
        std::unique_ptr<QMimeData> data = primarySelection->releaseMimeData();
        primarySelection.reset();
        QGuiApplication::clipboard()->setMimeData(data.release(), QClipboard::Selection);
    }
}

const QMimeData *WaylandClipboard::mimeData(QClipboard::Mode mode) const
{
    if (!waitForDevice(1000)) {
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

bool WaylandClipboard::waitForDevice(int timeoutMs) const
{
    if (m_device)
        return true;

    if (m_deviceRequestedTimer.elapsed() > timeoutMs)
        return false;

    qWarning() << "Waiting for Wayland clipboard for"
        << timeoutMs - m_deviceRequestedTimer.elapsed() << "ms";

    while (!m_device && m_deviceRequestedTimer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
    }

    if (m_device) {
        if (m_deviceRequestedTimer.elapsed() > 200) {
            qWarning() << "Activating Wayland clipboard took"
                << m_deviceRequestedTimer.elapsed() << "ms";
        }
        return true;
    }

    qCritical() << "Failed to activate Wayland clipboard";
    return false;
}

WaylandClipboard *WaylandClipboard::instance()
{
    static WaylandClipboard *clipboard = createInstance();
    return clipboard;
}

WaylandClipboard::~WaylandClipboard() = default;

#include "waylandclipboard.moc"

/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "waylandclipboard.h"

#include <QDebug>
#include <QFile>
#include <QFutureWatcher>
#include <QGuiApplication>

#include <QtWaylandClient/QWaylandClientExtension>

#include <qpa/qplatformnativeinterface.h>

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "qwayland-wlr-data-control-unstable-v1.h"

class DataControlDeviceManager : public QWaylandClientExtensionTemplate<DataControlDeviceManager>
        , public QtWayland::zwlr_data_control_manager_v1
{
    Q_OBJECT
public:
    DataControlDeviceManager()
        : QWaylandClientExtensionTemplate<DataControlDeviceManager>(2)
    {
    }

    ~DataControlDeviceManager()
    {
        if ( isInitialized() )
            destroy();
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

    bool hasFormat(const QString &format) const override
    {
        return m_receivedFormats.contains(format);
    }

protected:
    void zwlr_data_control_offer_v1_offer(const QString &mime_type) override
    {
        m_receivedFormats << mime_type;
    }

    QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;

private:
    static bool readData(int fd, QByteArray &data);
    QStringList m_receivedFormats;
};

QVariant DataControlOffer::retrieveData(const QString &mimeType, QVariant::Type type) const
{
    if (!hasFormat(mimeType)) {
        return QVariant();
    }
    Q_UNUSED(type)

    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        return QVariant();
    }

    auto t = const_cast<DataControlOffer *>(this);
    t->receive(mimeType, pipeFds[1]);

    close(pipeFds[1]);

    /*
     * Ideally we need to introduce a non-blocking QMimeData object
     * Or a non-blocking constructor to QMimeData with the mimetypes that are relevant
     *
     * However this isn't actually any worse than X.
     */

    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    auto display = static_cast<struct ::wl_display *>(native->nativeResourceForIntegration("wl_display"));
    wl_display_flush(display);

    QFile readPipe;
    if (readPipe.open(pipeFds[0], QIODevice::ReadOnly, QFile::AutoCloseHandle)) {
        QByteArray data;
        if (readData(pipeFds[0], data)) {
            return data;
        }
        close(pipeFds[0]);
    }
    return QVariant();
}

// reads data from a file descriptor with a timeout of 1 second
// true if data is read successfully
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
    DataControlSource();
    ~DataControlSource()
    {
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
    for (const QString &format : mimeData->formats()) {
        offer(format);
    }
    if (mimeData->hasText()) {
        // ensure GTK applications get this mimetype to avoid them discarding the offer
        offer(QStringLiteral("text/plain;charset=utf-8"));
    }
}

void DataControlSource::zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd)
{
    QFile c;
    QString send_mime_type = mime_type;
    if( send_mime_type == QStringLiteral("text/plain;charset=utf-8")
        && !m_mimeData->hasFormat(QStringLiteral("text/plain;charset=utf-8")) )
    {
        // if we get a request on the fallback mime, send the data from the original mime type
        send_mime_type = QStringLiteral("text/plain");
    }
    if (c.open(fd, QFile::WriteOnly, QFile::AutoCloseHandle)) {
        c.write(m_mimeData->data(send_mime_type));
        c.close();
    }
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
        new DataControlOffer(id);
        // this will become memory managed when we retrieve the selection event
        // a compositor calling data_offer without doing that would be a bug
    }

    void zwlr_data_control_device_v1_selection(struct ::zwlr_data_control_offer_v1 *id) override
    {
        if (!id) {
            m_receivedSelection.reset();
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5,12,5)
            auto deriv = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
#else
            auto deriv = static_cast<QtWayland::zwlr_data_control_offer_v1 *>(zwlr_data_control_offer_v1_get_user_data(id));
#endif
            auto offer = dynamic_cast<DataControlOffer *>(deriv); // dynamic because of the dual inheritance
            m_receivedSelection.reset(offer);
        }
        emit receivedSelectionChanged();
    }

    void zwlr_data_control_device_v1_primary_selection(struct ::zwlr_data_control_offer_v1 *id) override
    {
        if (!id) {
            m_receivedPrimarySelection.reset();
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5,12,5)
            auto deriv = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
#else
            auto deriv = static_cast<QtWayland::zwlr_data_control_offer_v1 *>(zwlr_data_control_offer_v1_get_user_data(id));
#endif
            auto offer = dynamic_cast<DataControlOffer *>(deriv); // dynamic because of the dual inheritance
            m_receivedPrimarySelection.reset(offer);
        }
        emit receivedPrimarySelectionChanged();
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
    : SystemClipboard(parent)
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
                emit changed(QClipboard::Clipboard);
            });
            connect(m_device.get(), &DataControlDevice::selectionChanged, this, [this]() {
                emit changed(QClipboard::Clipboard);
            });

            connect(m_device.get(), &DataControlDevice::receivedPrimarySelectionChanged, this, [this]() {
                emit changed(QClipboard::Selection);
            });
            connect(m_device.get(), &DataControlDevice::primarySelectionChanged, this, [this]() {
                emit changed(QClipboard::Selection);
            });

        } else {
            m_device.reset();
        }
    });
}

void WaylandClipboard::setMimeData(QMimeData *mime, QClipboard::Mode mode)
{
    if (!m_device) {
        return;
    }
    auto source = std::make_unique<DataControlSource>(m_manager->create_data_source(), mime);
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
        return m_device->selection() ? m_device->selection() : m_device->receivedSelection();
    } else if (mode == QClipboard::Selection) {
        return m_device->primarySelection() ? m_device->primarySelection() : m_device->receivedPrimarySelection();
    }
    return nullptr;
}

#include "waylandclipboard.moc"

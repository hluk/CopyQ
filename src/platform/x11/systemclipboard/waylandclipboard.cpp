/*
   Copyright (C) 2020 David Edmundson <davidedmundson@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the Lesser GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "waylandclipboard.h"

#include <QFile>
#include <QFutureWatcher>
#include <QPointer>
#include <QDebug>
#include <QGuiApplication>

#include <QtWaylandClient/QWaylandClientExtension>

#include <qpa/qplatformnativeinterface.h>

#include <unistd.h>

#include "qwayland-wlr-data-control-unstable-v1.h"

#include <sys/select.h>

class DataControlDeviceManager : public QWaylandClientExtensionTemplate<DataControlDeviceManager>
        , public QtWayland::zwlr_data_control_manager_v1
{
    Q_OBJECT
public:
    DataControlDeviceManager()
        : QWaylandClientExtensionTemplate<DataControlDeviceManager>(1)
    {
    }

    ~DataControlDeviceManager() {
        if ( isInitialized() )
            destroy();
    }
};

class DataControlOffer : public QMimeData, public QtWayland::zwlr_data_control_offer_v1
{
    Q_OBJECT
public:
    DataControlOffer(struct ::zwlr_data_control_offer_v1 *id):
        QtWayland::zwlr_data_control_offer_v1(id)
    {
    }

    ~DataControlOffer() {
        if ( isInitialized() )
            destroy();
    }

    QStringList formats() const override
    {
        return m_receivedFormats;
    }

    bool hasFormat(const QString &format) const override {
         return m_receivedFormats.contains(format);
    }
protected:
    void zwlr_data_control_offer_v1_offer(const QString &mime_type) override {
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
    Q_UNUSED(type);

    int pipeFds[2];
    if (pipe(pipeFds) != 0){
        return QVariant();
    }

    auto t = const_cast<DataControlOffer*>(this);
    t->receive(mimeType, pipeFds[1]);

    close(pipeFds[1]);

    /*
     * Ideally we need to introduce a non-blocking QMimeData object
     * Or a non-blocking constructor to QMimeData with the mimetypes that are relevant
     *
     * However this isn't actually any worse than X.
     */

    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    auto display = static_cast<struct ::wl_display*>(native->nativeResourceForIntegration("wl_display"));
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
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(fd, &readset);
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    Q_FOREVER {
        int ready = select(FD_SETSIZE, &readset, nullptr, nullptr, &timeout);
        if (ready < 0) {
            qWarning() << "DataControlOffer: select() failed";
            return false;
        } else if (ready == 0) {
            qWarning("DataControlOffer: timeout reading from pipe");
            return false;
        } else {
            char buf[4096];
            int n = read(fd, buf, sizeof buf);

            if (n < 0) {
                qWarning("DataControlOffer: read() failed");
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
    ~DataControlSource() {
        if ( isInitialized() )
            destroy();
    }

    QMimeData *mimeData() {
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
    const QString text = mimeData->text();
    if (!text.isEmpty()) {
        const auto data = text.toUtf8();
        mimeData->setData(QLatin1String("text/plain;charset=utf-8"), data);
        mimeData->setData(QLatin1String("STRING"), data);
        mimeData->setData(QLatin1String("TEXT"), data);
        mimeData->setData(QLatin1String("UTF8_STRING"), data);
    }

    for (const QString &format: mimeData->formats()) {
        offer(format);
    }
}

void DataControlSource::zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd)
{
    QFile c;
    if (c.open(fd, QFile::WriteOnly, QFile::AutoCloseHandle)) {
        c.write(m_mimeData->data(mime_type));
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
    {}

    ~DataControlDevice() {
        if ( isInitialized() )
            destroy();
    }

    void setSelection(std::unique_ptr<DataControlSource> selection, QClipboard::Mode mode);
    void setSelectionData(::zwlr_data_control_source_v1 *id, QClipboard::Mode mode);
    QMimeData *receivedSelection(QClipboard::Mode mode) {
        return m_receivedSelection[mode].get();
    }
    QMimeData *selection(QClipboard::Mode mode) {
        return m_selection[mode] ? m_selection[mode]->mimeData() : nullptr;
    }

Q_SIGNALS:
    void receivedSelectionChanged(QClipboard::Mode mode);
    void selectionChanged(QClipboard::Mode mode);
protected:
    void zwlr_data_control_device_v1_data_offer(struct ::zwlr_data_control_offer_v1 *id) override {
        new DataControlOffer(id);
        // this will become memory managed when we retrieve the selection event
        // a compositor calling data_offer without doing that would be a bug
    }

    void zwlr_data_control_device_v1_selection(struct ::zwlr_data_control_offer_v1 *id) override {
        receiveSelection(id, QClipboard::Clipboard);
    }

    void zwlr_data_control_device_v1_primary_selection(struct ::zwlr_data_control_offer_v1 *id) override {
        receiveSelection(id, QClipboard::Selection);
    }

private:
    void receiveSelection(struct ::zwlr_data_control_offer_v1 *id, QClipboard::Mode mode) {
        auto &receivedSelection = m_receivedSelection[mode];
        if(!id ) {
            receivedSelection.reset();
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5,12,5)
            auto deriv = QtWayland::zwlr_data_control_offer_v1::fromObject(id);
#else
            auto deriv = static_cast<QtWayland::zwlr_data_control_offer_v1 *>(zwlr_data_control_offer_v1_get_user_data(id));
#endif
            auto offer = dynamic_cast<DataControlOffer*>(deriv); // dynamic because of the dual inheritance
            receivedSelection.reset(offer);
        }
        emit receivedSelectionChanged(mode);
    }
    std::unique_ptr<DataControlSource> m_selection[2]; // selection set locally
    std::unique_ptr<DataControlOffer> m_receivedSelection[2]; // latest selection set from externally to here
};


void DataControlDevice::setSelection(std::unique_ptr<DataControlSource> selection, QClipboard::Mode mode)
{
    if (selection->isCancelled())
        return;

    m_selection[mode] = std::move(selection);
    connect(m_selection[mode].get(), &DataControlSource::cancelled, this, [this, mode]() {
        m_selection[mode].reset();
        Q_EMIT selectionChanged(mode);
    });
    setSelectionData(m_selection[mode]->object(), mode);
    Q_EMIT selectionChanged(mode);
}

void DataControlDevice::setSelectionData(::zwlr_data_control_source_v1 *id, QClipboard::Mode mode)
{
    if (mode == QClipboard::Clipboard) {
        set_selection(id);
    } else if (mode == QClipboard::Selection) {
        if (zwlr_data_control_device_v1_get_version(object()) >=
                ZWLR_DATA_CONTROL_DEVICE_V1_SET_PRIMARY_SELECTION_SINCE_VERSION) {
            set_primary_selection(id);
        }
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
            auto seat = static_cast<struct ::wl_seat*>(native->nativeResourceForIntegration("wl_seat"));
            if (!seat) {
                return;
            }

            m_device.reset(new DataControlDevice(m_manager->get_data_device(seat)));

            connect(m_device.get(), &DataControlDevice::receivedSelectionChanged, this, [this](QClipboard::Mode mode) {
                    emit changed(mode);
            });
            connect(m_device.get(), &DataControlDevice::selectionChanged, this, [this](QClipboard::Mode mode) {
                    emit changed(mode);
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
    std::unique_ptr<DataControlSource> source(new DataControlSource(m_manager->create_data_source(), mime));
    m_device->setSelection(std::move(source), mode);
}

void WaylandClipboard::clear(QClipboard::Mode mode)
{
    if (!m_device) {
        return;
    }
    m_device->setSelectionData(nullptr, mode);
}

const QMimeData *WaylandClipboard::mimeData(QClipboard::Mode mode) const
{
    if (!m_device) {
        return nullptr;
    }
    // return our locally set selection if it's not cancelled to avoid copying data to ourselves
    return m_device->selection(mode) ? m_device->selection(mode) : m_device->receivedSelection(mode);
}

#include "waylandclipboard.moc"

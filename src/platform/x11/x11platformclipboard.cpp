// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <memory>

#include "x11platformclipboard.h"

#include "x11info.h"

#include "common/clipboarddataguard.h"
#include "common/common.h"
#include "common/mimetypes.h"
#include "common/timer.h"

#ifdef HAS_KGUIADDONS
#   include <KSystemClipboard>
#else
#   include "systemclipboard/waylandclipboard.h"
using KSystemClipboard = WaylandClipboard;
#endif

#include <QDataStream>

#ifdef COPYQ_WITH_X11
#   include <X11/Xlib.h>
#   include <X11/Xatom.h>
#endif

#include <QClipboard>
#include <QMetaType>
#include <QMimeData>
#include <QPointer>
#include <optional>

namespace {

Q_LOGGING_CATEGORY(logClipboard, "copyq.clipboard")
Q_LOGGING_CATEGORY(logClipboardGnome, "copyq.clipboard.gnome")

constexpr auto minCheckAgainIntervalMs = 50;
constexpr auto maxCheckAgainIntervalMs = 500;
constexpr auto maxRetryCount = 3;
const auto gnomeClipboardService = QStringLiteral("com.github.hluk.copyq.GnomeClipboard");
const auto gnomeClipboardPath = QStringLiteral("/com/github/hluk/copyq/GnomeClipboard");
const auto gnomeClipboardInterface = QStringLiteral("com.github.hluk.CopyQ.GnomeClipboard1");
const auto gnomeClipboardClientPath = QStringLiteral("/com/github/hluk/copyq/GnomeClipboardClient");
const auto gnomeClipboardClientInterface = QStringLiteral("com.github.hluk.CopyQ.GnomeClipboardClient1");
constexpr int gnomeClipboardTypeClipboard = 0;
constexpr int gnomeClipboardTypePrimary = 1;
constexpr int gnomeClipboardTypeBoth = 2;

int gnomeClipboardType(ClipboardMode mode)
{
    return mode == ClipboardMode::Selection ? gnomeClipboardTypePrimary : gnomeClipboardTypeClipboard;
}

/// Return true only if selection is incomplete, i.e. mouse button or shift key is pressed.
bool isSelectionIncomplete()
{
#ifdef COPYQ_WITH_X11
    if (!X11Info::isPlatformX11())
        return false;

    auto display = X11Info::display();
    if (!display)
        return false;

    // If mouse button or shift is pressed then assume that user is selecting text.
    XButtonEvent event{};
    XQueryPointer(display, DefaultRootWindow(display),
                  &event.root, &event.window,
                  &event.x_root, &event.y_root,
                  &event.x, &event.y,
                  &event.state);

    return event.state & (Button1Mask | ShiftMask);
#else
    return false;
#endif
}

QVariant unwrapDbusValue(QVariant value)
{
    if (value.canConvert<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();

    if (value.userType() == QMetaType::QByteArray)
        return value;

    if (value.canConvert<QByteArray>()) {
        const auto bytes = value.toByteArray();
        if (!bytes.isNull())
            return bytes;
    }

    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        const auto argument = value.value<QDBusArgument>();
        if (argument.currentType() == QDBusArgument::VariantType) {
            QVariant nested;
            argument >> nested;
            return unwrapDbusValue(nested);
        }
        if (argument.currentType() == QDBusArgument::ArrayType) {
            QByteArray bytes;
            argument.beginArray();
            while (!argument.atEnd()) {
                QVariant element;
                argument >> element;
                bytes.append(static_cast<char>(element.toUInt()));
            }
            argument.endArray();
            return bytes;
        }
    }

    return value;
}

QVariantMap variantToMap(const QVariant &value)
{
    if (value.canConvert<QVariantMap>())
        return value.toMap();

    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        const auto argument = value.value<QDBusArgument>();
        if (argument.currentType() == QDBusArgument::MapType) {
            QVariantMap map;
            argument.beginMap();
            while (!argument.atEnd()) {
                QString key;
                QVariant entryValue;
                argument.beginMapEntry();
                argument >> key >> entryValue;
                argument.endMapEntry();
                map.insert(key, unwrapDbusValue(entryValue));
            }
            argument.endMap();
            return map;
        }
    }

    return {};
}

QVariantMap normalizeClipboardDataMap(const QVariantMap &dataMap)
{
    QVariantMap data;
    for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it) {
        QVariant value = unwrapDbusValue(it.value());
        if (value.userType() == QMetaType::QByteArray) {
            data.insert(it.key(), value);
        } else if (value.userType() == QMetaType::QString) {
            data.insert(it.key(), value.toString().toUtf8());
        }
    }
    return data;
}

std::optional<QPair<QString, QVariant>> preferredClipboardFormatValue(const QVariantMap &dataMap)
{
    static const QStringList preferredFormats = {
        QStringLiteral("text/plain;charset=utf-8"),
        QStringLiteral("text/plain"),
        QStringLiteral("text/uri-list"),
        QStringLiteral("image/png"),
        QStringLiteral("image/bmp"),
        QStringLiteral("image/jpeg"),
        QStringLiteral("image/gif"),
        QStringLiteral("image/svg+xml"),
        QStringLiteral("text/html"),
    };

    if (dataMap.isEmpty())
        return std::nullopt;

    for (const auto &format : preferredFormats) {
        const auto it = dataMap.constFind(format);
        if (it != dataMap.constEnd())
            return QPair<QString, QVariant>(format, it.value());
    }

    for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it) {
        if (!it.key().startsWith(QLatin1String(COPYQ_MIME_PREFIX)))
            return QPair<QString, QVariant>(it.key(), it.value());
    }

    const auto it = dataMap.constBegin();
    return QPair<QString, QVariant>(it.key(), it.value());
}

QVariantMap dataMapFromMimeData(const QMimeData *mimeData)
{
    if (mimeData == nullptr)
        return {};

    QVariantMap data;
    for (const auto &format : mimeData->formats())
        data.insert(format, mimeData->data(format));

    if (mimeData->hasText()) {
        const QByteArray bytes = mimeData->text().toUtf8();
        data.insert(mimeText, bytes);
        data.insert(mimeTextUtf8, bytes);
    }
    return data;
}

} // namespace

class GnomeClipboardExtensionClient final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.github.hluk.CopyQ.GnomeClipboardClient1")
public:
    explicit GnomeClipboardExtensionClient(QObject *parent)
        : QObject(parent)
        , m_connection(QDBusConnection::sessionBus())
        , m_serviceWatcher(
                gnomeClipboardService, m_connection,
                QDBusServiceWatcher::WatchForRegistration
                | QDBusServiceWatcher::WatchForUnregistration,
                this)
    {
        if (!m_connection.isConnected()) {
            qCWarning(logClipboardGnome)
                << "Failed to register GNOME clipboard client: DBus is not available";
            return;
        }

        m_registeredObject = m_connection.registerObject(
            gnomeClipboardClientPath, this, QDBusConnection::ExportScriptableSlots);
        if (!m_registeredObject) {
            const auto error = m_connection.lastError();
            qCInfo(logClipboardGnome)
                << "Failed to register GNOME clipboard client:"
                << error.name() << "-" << error.message();
        }

        connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered,
                this, &GnomeClipboardExtensionClient::onServiceRegistered);
        connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
                this, &GnomeClipboardExtensionClient::onServiceUnregistered);
        m_serviceWatcherReady = m_connection.interface()
            && m_connection.interface()->isServiceRegistered(gnomeClipboardService);
    }

    bool isConnected() const
    {
        return m_registeredObject && m_connection.isConnected() && m_serviceWatcherReady;
    }

    bool isRegisteredWithExtension() const { return m_registeredWithExtension; }

    ~GnomeClipboardExtensionClient() override
    {
        unregisterClient();
        if (m_registeredObject)
            m_connection.unregisterObject(gnomeClipboardClientPath);
    }

    void registerClipboardTypes(int clipboardTypes)
    {
        m_clipboardTypes = clipboardTypes;
        if (m_serviceWatcherReady)
            registerClient();
    }

    void unregisterClipboardTypes()
    {
        unregisterClient();
    }

    QStringList fetchClipboardFormats(ClipboardMode mode) const
    {
        if (!m_serviceWatcherReady)
            return {};

        auto message = QDBusMessage::createMethodCall(
            gnomeClipboardService, gnomeClipboardPath, gnomeClipboardInterface,
            QStringLiteral("GetClipboardFormats"));
        message << gnomeClipboardType(mode);
        const QDBusMessage reply = m_connection.call(message);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qCWarning(logClipboardGnome)
                << "Failed to fetch clipboard formats from GNOME extension:"
                << reply.errorName() << "-" << reply.errorMessage();
            return {};
        }

        const auto arguments = reply.arguments();
        if (arguments.isEmpty() || !arguments.first().canConvert<QStringList>()) {
            qCWarning(logClipboardGnome)
                << "Failed to fetch clipboard formats from GNOME extension: invalid reply";
            return {};
        }
        return arguments.first().toStringList();
    }

    QVariantMap fetchClipboardData(ClipboardMode mode, const QStringList &formats) const
    {
        if (!m_serviceWatcherReady)
            return {};

        auto message = QDBusMessage::createMethodCall(
            gnomeClipboardService, gnomeClipboardPath, gnomeClipboardInterface,
            QStringLiteral("GetClipboardData"));
        message << gnomeClipboardType(mode) << formats;
        const QDBusMessage reply = m_connection.call(message);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qCWarning(logClipboardGnome)
                << "Failed to fetch clipboard data from GNOME extension:"
                << reply.errorName() << "-" << reply.errorMessage();
            return {};
        }

        const auto arguments = reply.arguments();
        if (arguments.isEmpty()) {
            qCWarning(logClipboardGnome)
                << "Failed to fetch clipboard data from GNOME extension: empty reply";
            return {};
        }

        auto data = variantToMap(arguments.first());
        for (auto it = data.begin(); it != data.end(); ++it) {
            it.value() = unwrapDbusValue(it.value());
        }
        return data;
    }

    void setClipboardData(ClipboardMode mode, const QString &format, const QVariant &value)
    {
        if (!m_serviceWatcherReady)
            return;

        auto message = QDBusMessage::createMethodCall(
            gnomeClipboardService, gnomeClipboardPath, gnomeClipboardInterface,
            QStringLiteral("SetClipboardData"));
        message << gnomeClipboardType(mode) << format << QVariant::fromValue(QDBusVariant(value));
        QDBusReply<void> reply = m_connection.call(message);
        if (!reply.isValid()) {
            qCWarning(logClipboardGnome)
                << "Failed to set clipboard data through GNOME extension:"
                << format << reply.error().name() << "-" << reply.error().message();
        }
    }

signals:
    void clipboardChanged(int clipboardType);

public slots:
    Q_SCRIPTABLE void ClipboardChanged(int clipboardType)
    {
        emit clipboardChanged(clipboardType);
    }

private:
    void registerClient()
    {
        auto message = QDBusMessage::createMethodCall(
            gnomeClipboardService, gnomeClipboardPath, gnomeClipboardInterface,
            QStringLiteral("RegisterClipboardClient"));
        message << m_clipboardTypes;
        QDBusReply<void> reply = m_connection.call(message);
        if (!reply.isValid()) {
            m_registeredWithExtension = false;
            qCWarning(logClipboardGnome)
                << "Failed to register with GNOME extension service:"
                << reply.error().name() << "-" << reply.error().message();
        } else {
            m_registeredWithExtension = true;
            qCDebug(logClipboardGnome)
                << "Registered with GNOME extension service, clipboardTypes:"
                << m_clipboardTypes;
        }
    }

    void unregisterClient()
    {
        if (!m_registeredWithExtension)
            return;

        auto message = QDBusMessage::createMethodCall(
            gnomeClipboardService, gnomeClipboardPath, gnomeClipboardInterface,
            QStringLiteral("UnregisterClipboardClient"));
        QDBusReply<void> reply = m_connection.call(message);
        if (!reply.isValid()) {
            qCWarning(logClipboardGnome)
                << "Failed to unregister from GNOME extension service:"
                << reply.error().message();
        } else {
            m_registeredWithExtension = false;
        }
    }

    void onServiceRegistered(const QString &name)
    {
        if (name == gnomeClipboardService) {
            m_serviceWatcherReady = true;
            registerClient();
        }
    }

    void onServiceUnregistered(const QString &name)
    {
        if (name == gnomeClipboardService) {
            m_serviceWatcherReady = false;
            m_registeredWithExtension = false;
        }
    }

    QDBusConnection m_connection;
    QDBusServiceWatcher m_serviceWatcher;
    int m_clipboardTypes = gnomeClipboardTypeClipboard;
    bool m_registeredObject = false;
    bool m_registeredWithExtension = false;
    bool m_serviceWatcherReady = false;
};

class GnomeExtensionMimeData final : public QMimeData
{
public:
    explicit GnomeExtensionMimeData(const GnomeClipboardExtensionClient *client, ClipboardMode mode)
        : m_client(client)
        , m_mode(mode)
    {}

    QStringList formats() const override
    {
        return m_client ? m_client->fetchClipboardFormats(m_mode) : QStringList{};
    }

    bool hasFormat(const QString &mimeType) const override
    {
        if (!m_client)
            return false;
        const auto formats = m_client->fetchClipboardFormats(m_mode);
        if (formats.contains(mimeType))
            return true;
        const auto data = m_client->fetchClipboardData(m_mode, {mimeType});
        return data.contains(mimeType);
    }

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QVariant retrieveData(const QString &mimeType, QMetaType preferredType) const override
#else
    QVariant retrieveData(const QString &mimeType, QVariant::Type preferredType) const override
#endif
    {
        Q_UNUSED(preferredType)
        if (!m_client)
            return {};

        auto data = m_client->fetchClipboardData(m_mode, {mimeType});
        const auto value = data.value(mimeType);
        if (value.userType() == QMetaType::QByteArray)
            return value;
        if (value.userType() == QMetaType::QString)
            return value.toString().toUtf8();
        if (value.canConvert<QByteArray>())
            return value.toByteArray();
        return {};
    }

private:
    const GnomeClipboardExtensionClient *m_client = nullptr;
    ClipboardMode m_mode = ClipboardMode::Clipboard;
};

X11PlatformClipboard::X11PlatformClipboard()
{
    m_clipboardData.mode = ClipboardMode::Clipboard;
    m_selectionData.mode = ClipboardMode::Selection;

    if ( !X11Info::isPlatformX11() ) {
        // Create Wayland clipboard instance so it can start receiving new data.
        KSystemClipboard::instance();

        // Try to register with CopyQ GNOME extension.
        m_gnomeClipboardExtensionClient = std::make_unique<GnomeClipboardExtensionClient>(this);
        connect(
            m_gnomeClipboardExtensionClient.get(), &GnomeClipboardExtensionClient::clipboardChanged,
            this, &X11PlatformClipboard::onGnomeClipboardChanged );
        if (!m_gnomeClipboardExtensionClient->isConnected())
            m_gnomeClipboardExtensionClient.reset();
    }
}

X11PlatformClipboard::~X11PlatformClipboard() = default;

void X11PlatformClipboard::updateMonitoringSubscription(
        const QStringList &formats, ClipboardModeMask modes)
{
    m_clipboardData.formats = formats;

    // Avoid asking apps for bigger data when mouse selection changes.
    // This could make the app hang for a moment.
    m_selectionData.formats = QStringList{mimeText, mimeTextUtf8};
    for (auto &format : formats) {
        if (!format.startsWith(QLatin1String("image/")) && !format.startsWith(QLatin1String("text/")))
            m_selectionData.formats.append(format);
    }

    m_clipboardData.enabled = modes.testFlag(ClipboardModeFlag::Clipboard);
    m_selectionData.enabled = modes.testFlag(ClipboardModeFlag::Selection);

    if ( m_selectionData.enabled && !QGuiApplication::clipboard()->supportsSelection() ) {
        qCWarning(logClipboard) << "X11 selection is not supported, disabling.";
        m_selectionData.enabled = false;
    }

    if (!X11Info::isPlatformX11() && m_monitoring && isGnomeExtensionAvailable()) {
        m_gnomeClipboardExtensionClient->registerClipboardTypes(toGnomeClipboardTypes(modes));
    }
}

void X11PlatformClipboard::startMonitoringBackend(
        const QStringList &formats, ClipboardModeMask modes)
{
    updateMonitoringSubscription(formats, modes);

    if ( !X11Info::isPlatformX11() ) {
        connect(KSystemClipboard::instance(), &KSystemClipboard::changed,
                this, [this](QClipboard::Mode mode){ onClipboardChanged(mode); });
        if (isGnomeExtensionAvailable())
            m_gnomeClipboardExtensionClient->registerClipboardTypes(toGnomeClipboardTypes(modes));
    }

    // Ignore the initial clipboard content since
    // it won't have the correct owner's window title.
    m_clipboardData.ignoreNext = true;
    m_selectionData.ignoreNext = true;
    QTimer::singleShot(5000, this, [this](){
        m_clipboardData.ignoreNext = false;
        m_selectionData.ignoreNext = false;
    });

    for (auto clipboardData : {&m_clipboardData, &m_selectionData}) {
        clipboardData->owner.clear();
        clipboardData->newOwner.clear();
        if ( X11Info::isPlatformX11() ) {
            updateClipboardData(clipboardData);
            useNewClipboardData(clipboardData);
        }
    }

    initSingleShotTimer( &m_timerCheckAgain, 0, this, &X11PlatformClipboard::check );

    initSingleShotTimer( &m_clipboardData.timerEmitChange, 0, this, [this](){
        useNewClipboardData(&m_clipboardData);
    } );

    initSingleShotTimer( &m_selectionData.timerEmitChange, 0, this, [this](){
        useNewClipboardData(&m_selectionData);
    } );

    m_monitoring = true;

    DummyClipboard::startMonitoringBackend(formats, modes);
}

void X11PlatformClipboard::stopMonitoringBackend()
{
    if (!m_monitoring)
        return;

    m_timerCheckAgain.stop();
    m_clipboardData.timerEmitChange.stop();
    m_selectionData.timerEmitChange.stop();
    m_monitoring = false;

    if ( !X11Info::isPlatformX11() ) {
        disconnect(KSystemClipboard::instance(), nullptr, this, nullptr);
        if (isGnomeExtensionAvailable())
            m_gnomeClipboardExtensionClient->unregisterClipboardTypes();
    }

    DummyClipboard::stopMonitoringBackend();
}

QVariantMap X11PlatformClipboard::data(ClipboardMode mode, const QStringList &formats) const
{
    if (!m_monitoring) {
        if (isGnomeExtensionAvailable()) {
            return m_gnomeClipboardExtensionClient->fetchClipboardData(mode, formats);
        }
        return DummyClipboard::data(mode, formats);
    }

    const auto &clipboardData = mode == ClipboardMode::Clipboard ? m_clipboardData : m_selectionData;

    auto data = clipboardData.data;
    if ( !data.contains(mimeOwner) )
        data[mimeWindowTitle] = clipboardData.owner.toUtf8();
    return data;
}

const QMimeData *X11PlatformClipboard::mimeData(ClipboardMode mode) const
{
    if (isGnomeExtensionAvailable()) {
        std::unique_ptr<QMimeData> &ptr = (mode == ClipboardMode::Clipboard)
            ? m_gnomeClipboardMimeData : m_gnomeSelectionMimeData;
        if (!ptr) {
            ptr = std::make_unique<GnomeExtensionMimeData>(
                    m_gnomeClipboardExtensionClient.get(), mode);
        }
        return ptr.get();
    }

    return DummyClipboard::mimeData(mode);
}

void X11PlatformClipboard::setData(ClipboardMode mode, const QVariantMap &dataMap)
{
    if ( X11Info::isPlatformX11() ) {
        // WORKAROUND: Avoid getting X11 warning "QXcbClipboard: SelectionRequest too old".
        QCoreApplication::processEvents();

        // WORKAROUND: XWayland on GNOME does not handle UTF-8 text properly.
        if ( dataMap.contains(mimeTextUtf8) && qgetenv("XAUTHORITY").contains("mutter-Xwayland") ) {
            auto dataMap2 = dataMap;
            dataMap2[mimeText] = dataMap[mimeTextUtf8];
            DummyClipboard::setData(mode, dataMap2);
        } else {
            DummyClipboard::setData(mode, dataMap);
        }
    } else {
        auto data = createMimeData(dataMap);
        const auto qmode = modeToQClipboardMode(mode);
        if (isGnomeExtensionAvailable()) {
            setGnomeExtensionClipboardData(mode, dataMap);
            DummyClipboard::setData(mode, dataMap);
        } else {
            DummyClipboard::setData(mode, dataMap);
            // WORKAROUND: KGuiAddons does not handle UTF-8 text properly.
            // This unfortunately overrides "text/plain" with
            // "text/plain;charset=utf-8" (these can differ).
            // See: https://invent.kde.org/frameworks/kguiaddons/-/merge_requests/184
            if (dataMap.contains(mimeTextUtf8))
                data->setText(dataMap.value(mimeTextUtf8).toString());
            else
                KSystemClipboard::instance()->setMimeData(data, qmode);
        }
    }
}

void X11PlatformClipboard::setRawData(ClipboardMode mode, QMimeData *mimeData)
{
    if (isGnomeExtensionAvailable())
        setGnomeExtensionClipboardData(mode, dataMapFromMimeData(mimeData));
    DummyClipboard::setRawData(mode, mimeData);
}

const QMimeData *X11PlatformClipboard::rawMimeData(ClipboardMode mode) const
{
    if ( !X11Info::isPlatformX11() ) {
        const auto data = KSystemClipboard::instance()->mimeData( modeToQClipboardMode(mode) );
        if (data != nullptr)
            return data;
    }
    return DummyClipboard::rawMimeData(mode);
}

void X11PlatformClipboard::onChanged(int mode)
{
    auto &clipboardData = mode == QClipboard::Clipboard ? m_clipboardData : m_selectionData;
    if (!clipboardData.enabled)
        return;

    ++clipboardData.sequenceNumber;
    clipboardData.ignoreNext = false;

    // Store the current window title right after the clipboard/selection changes.
    // This makes sure that the title points to the correct clipboard/selection
    // owner most of the times.
    clipboardData.newOwner = m_clipboardOwner;

    if (mode == QClipboard::Selection) {
        // Omit checking selection too fast.
        if ( m_timerCheckAgain.isActive() ) {
            qCDebug(logClipboard) << "Postponing fast selection change";
            return;
        }

        if ( isSelectionIncomplete() ) {
            qCDebug(logClipboard) << "Selection is incomplete";
            if ( !m_timerCheckAgain.isActive()
                 || minCheckAgainIntervalMs < m_timerCheckAgain.remainingTime() )
            {
                m_timerCheckAgain.start(minCheckAgainIntervalMs);
            }
            return;
        }
    }

    if (m_clipboardData.cloningData || m_selectionData.cloningData)
        return;

    updateClipboardData(&clipboardData);

    checkAgainLater(true, 0);
}

void X11PlatformClipboard::check()
{
    if (m_clipboardData.cloningData || m_selectionData.cloningData) {
        m_timerCheckAgain.start(minCheckAgainIntervalMs);
        return;
    }

    m_timerCheckAgain.stop();

    updateClipboardData(&m_clipboardData);
    updateClipboardData(&m_selectionData);

    if ( m_timerCheckAgain.isActive() )
        return;

    const bool changed = m_clipboardData.timerEmitChange.isActive()
        || m_selectionData.timerEmitChange.isActive();

    // Check clipboard and selection again if some signals where
    // not delivered or older data was received after new one.
    const int interval = m_timerCheckAgain.interval() * 2 + minCheckAgainIntervalMs;
    checkAgainLater(changed, interval);
}

void X11PlatformClipboard::updateClipboardData(X11PlatformClipboard::ClipboardData *clipboardData)
{
    if (!clipboardData->enabled)
        return;

    if ( isSelectionIncomplete() ) {
        m_timerCheckAgain.start(minCheckAgainIntervalMs);
        return;
    }

    ClipboardDataGuard data( mimeData(clipboardData->mode), &clipboardData->sequenceNumber );

    // Retry to retrieve clipboard data few times.
    if (data.isExpired()) {
        if ( !X11Info::isPlatformX11() )
            return;

        if ( rawMimeData(clipboardData->mode) )
            return;

        if (clipboardData->retry < maxRetryCount) {
            ++clipboardData->retry;
            m_timerCheckAgain.start(clipboardData->retry * maxCheckAgainIntervalMs);
        }

        qCWarning(logClipboard)
            << "Failed to retrieve"
            << (clipboardData->mode == ClipboardMode::Clipboard ? "clipboard" : "selection")
            << "data, try" << clipboardData->retry << "of" << maxRetryCount;

        return;
    }
    clipboardData->retry = 0;

    const QByteArray newDataTimestampData = data.data(QStringLiteral("TIMESTAMP"));
    quint32 newDataTimestamp = 0;
    if ( !newDataTimestampData.isEmpty() ) {
        QDataStream stream(newDataTimestampData);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream >> newDataTimestamp;
        if (stream.status() != QDataStream::Ok)
            newDataTimestamp = 0;
    }

    // In case there is a valid timestamp, omit update if the timestamp and
    // text did not change.
    if ( newDataTimestamp != 0 && clipboardData->newDataTimestamp == newDataTimestamp ) {
        const QVariantMap newData = cloneData(data, {mimeText});
        if (data.isExpired() || newData.value(mimeText) == clipboardData->newData.value(mimeText))
            return;
    }

    clipboardData->timerEmitChange.stop();
    clipboardData->cloningData = true;
    const bool isDataSecret = isHidden(*data.mimeData());
    clipboardData->newData = cloneData(data, clipboardData->formats);
    if (isDataSecret)
        clipboardData->newData[mimeSecret] = QByteArrayLiteral("1");
    clipboardData->cloningData = false;

    if (data.isExpired()) {
        m_timerCheckAgain.start(0);
        return;
    }

    // Update only if the data changed.
    if ( clipboardData->data == clipboardData->newData )
        return;

    clipboardData->newDataTimestamp = newDataTimestamp;
    clipboardData->timerEmitChange.start();
}

void X11PlatformClipboard::useNewClipboardData(X11PlatformClipboard::ClipboardData *clipboardData)
{
    qCDebug(logClipboard)
        << (clipboardData->mode == ClipboardMode::Clipboard ? "Clipboard" : "Selection")
        << "CHANGED, owner:" << clipboardData->newOwner;

    clipboardData->data = clipboardData->newData;
    clipboardData->owner = clipboardData->newOwner;
    clipboardData->timerEmitChange.stop();
    if (clipboardData->ignoreNext)
        clipboardData->ignoreNext = false;
    else
        emitConnectionChanged(clipboardData->mode);
}

void X11PlatformClipboard::checkAgainLater(bool clipboardChanged, int interval)
{
    if (interval < maxCheckAgainIntervalMs)
        m_timerCheckAgain.start(interval);
    else if (clipboardChanged)
        m_timerCheckAgain.start(maxCheckAgainIntervalMs);
}

void X11PlatformClipboard::onGnomeClipboardChanged(int clipboardType)
{
    if (clipboardType == gnomeClipboardTypeClipboard) {
        qCDebug(logClipboardGnome) << "GNOME Clipboard change";
        onChanged(QClipboard::Clipboard);
    } else {
        qCDebug(logClipboardGnome) << "GNOME Selection change";
        onChanged(QClipboard::Selection);
    }
}

bool X11PlatformClipboard::isGnomeExtensionAvailable() const
{
    return m_gnomeClipboardExtensionClient
        && m_gnomeClipboardExtensionClient->isConnected();
}

void X11PlatformClipboard::setGnomeExtensionClipboardData(ClipboardMode mode, const QVariantMap &dataMap) const
{
    const QVariantMap normalizedDataMap = normalizeClipboardDataMap(dataMap);
    if (const auto preferred = preferredClipboardFormatValue(normalizedDataMap))
        m_gnomeClipboardExtensionClient->setClipboardData(mode, preferred->first, preferred->second);
}

int X11PlatformClipboard::toGnomeClipboardTypes(ClipboardModeMask modes) const
{
    const bool clipboard = modes.testFlag(ClipboardModeFlag::Clipboard);
    const bool selection = modes.testFlag(ClipboardModeFlag::Selection);
    if (clipboard && selection)
        return gnomeClipboardTypeBoth;
    if (selection)
        return gnomeClipboardTypePrimary;
    return gnomeClipboardTypeClipboard;
}

#include "x11platformclipboard.moc"

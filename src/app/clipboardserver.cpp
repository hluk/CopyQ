/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "clipboardserver.h"

#include "app/remoteprocess.h"
#include "common/appconfig.h"
#include "common/arguments.h"
#include "common/clientsocket.h"
#include "common/client_server.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/monitormessagecode.h"
#include "gui/clipboardbrowser.h"
#include "gui/commanddialog.h"
#include "gui/configtabshortcuts.h"
#include "gui/iconfactory.h"
#include "gui/mainwindow.h"
#include "item/itemfactory.h"
#include "item/serialize.h"
#include "scriptable/scriptableworker.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProxyStyle>
#include <QScreen>
#include <QSessionManager>
#include <QThread>

#ifdef NO_GLOBAL_SHORTCUTS
struct QxtGlobalShortcut {};
#else
#include "../qxt/qxtglobalshortcut.h"
#endif

namespace {

const double lowResDpi = 96.0;

int defaultDpi()
{
#if QT_VERSION < 0x050000
    static const qreal dpi = QWidget().logicalDpiX();
#else
    static const qreal dpi = QApplication::primaryScreen()->logicalDotsPerInch();
#endif
    return dpi;
}

int dpiForWidget(const QWidget *widget)
{
    return widget ? widget->logicalDpiX() : defaultDpi();
}

// Round down to nearest quarter (1.0, 1.25, 1.5, 2.0 ...).
double normalizeFactor(qreal f)
{
    return qMax(1.0, static_cast<int>(f * 4.0) / 4.0);
}

int fromPixels(int pixels, const QWidget *widget)
{
    const qreal f = dpiForWidget(widget) / lowResDpi;
    return pixels * normalizeFactor(f);
}

bool areIconsTooSmall()
{
    const int idealIconSize = fromPixels(16, NULL);
    return smallIconSize() < idealIconSize;
}

class ApplicationStyle : public QProxyStyle {
public:
    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
    {
        return fromPixels(QProxyStyle::pixelMetric(metric, option, widget), widget);
    }
};

} // namespace

ClipboardServer::ClipboardServer(int &argc, char **argv, const QString &sessionName)
    : QObject()
    , App(createPlatformNativeInterface()->createServerApplication(argc, argv),
          sessionName)
    , m_wnd(NULL)
    , m_monitor(NULL)
    , m_shortcutActions()
    , m_clientThreads()
    , m_ignoreKeysTimer()
{
    const QString serverName = clipboardServerName();
    Server *server = new Server(serverName, this);

    if ( server->isListening() ) {
        ::createSessionMutex();
        restoreSettings(true);
        COPYQ_LOG("Server \"" + serverName + "\" started.");
    } else {
        restoreSettings(false);
        COPYQ_LOG("Server \"" + serverName + "\" already running!");
        log( tr("CopyQ server is already running."), LogWarning );
        exit(0);
        return;
    }

    QApplication::setQuitOnLastWindowClosed(false);

    if (areIconsTooSmall())
        QApplication::setStyle(new ApplicationStyle);

    m_itemFactory = new ItemFactory(this);
    m_wnd = new MainWindow(m_itemFactory);

    connect( server, SIGNAL(newConnection(Arguments,ClientSocket*)),
             this, SLOT(doCommand(Arguments,ClientSocket*)) );

    connect( qApp, SIGNAL(aboutToQuit()),
             this, SLOT(onAboutToQuit()));

    connect( qApp, SIGNAL(commitDataRequest(QSessionManager&)),
             this, SLOT(onCommitData(QSessionManager&)) );

    connect( m_wnd, SIGNAL(changeClipboard(QVariantMap,QClipboard::Mode)),
             this, SLOT(changeClipboard(QVariantMap,QClipboard::Mode)) );

    connect( m_wnd, SIGNAL(requestExit()),
             this, SLOT(maybeQuit()) );

    loadSettings();

    // notify window if configuration changes
    connect( m_wnd, SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );

#ifndef NO_GLOBAL_SHORTCUTS
    connect( m_wnd, SIGNAL(commandsSaved()),
             this, SLOT(createGlobalShortcuts()) );
    createGlobalShortcuts();
#endif

    // Allow to run at least few client and internal threads concurrently.
    m_clientThreads.setMaxThreadCount( qMax(m_clientThreads.maxThreadCount(), 8) );

    // run clipboard monitor
    startMonitoring();

    qApp->installEventFilter(this);

    server->start();

    // Ignore global shortcut key presses in any widget.
    m_ignoreKeysTimer.setInterval(100);
    m_ignoreKeysTimer.setSingleShot(true);
}

ClipboardServer::~ClipboardServer()
{
    removeGlobalShortcuts();
    delete m_wnd;
}

void ClipboardServer::stopMonitoring()
{
    if (m_monitor == NULL)
        return;

    COPYQ_LOG("Clipboard Monitor: Terminating");

    m_monitor->disconnect();
    delete m_monitor;
    m_monitor = NULL;

    COPYQ_LOG("Clipboard Monitor: Terminated");
}

void ClipboardServer::startMonitoring()
{
    COPYQ_LOG("Starting monitor.");

    if ( m_monitor == NULL ) {
        m_monitor = new RemoteProcess(this);
        connect( m_monitor, SIGNAL(newMessage(QByteArray)),
                 this, SLOT(newMonitorMessage(QByteArray)) );
        connect( m_monitor, SIGNAL(connectionError()),
                 this, SLOT(monitorConnectionError()) );
        connect( m_monitor, SIGNAL(connected()),
                 this, SLOT(loadMonitorSettings()) );

        const QString name = serverName("m");
        m_monitor->start( name, QStringList("monitor") << name );
    }
}

void ClipboardServer::loadMonitorSettings()
{
    if ( !isMonitoring() ) {
        COPYQ_LOG("Cannot configure monitor!");
        return;
    }

    COPYQ_LOG("Configuring monitor.");

    QVariantMap settings;
    settings["formats"] = m_itemFactory->formatsToSave();
#ifdef COPYQ_WS_X11
    settings["check_selection"] = AppConfig().option<Config::check_selection>();
#endif

    QByteArray settingsData;
    QDataStream settingsOut(&settingsData, QIODevice::WriteOnly);
    settingsOut << settings;

    m_monitor->writeMessage(settingsData, MonitorSettings);
}

bool ClipboardServer::isMonitoring()
{
    return m_monitor != NULL && m_monitor->isConnected();
}

void ClipboardServer::removeGlobalShortcuts()
{
    foreach (QxtGlobalShortcut *s, m_shortcutActions.keys())
        delete s;
    m_shortcutActions.clear();
}

void ClipboardServer::createGlobalShortcuts()
{
    removeGlobalShortcuts();

    QList<QKeySequence> usedShortcuts;

    foreach ( const Command &command, loadCommands() ) {
        if ( !command.globalShortcuts.contains("DISABLED") ) {
            foreach (const QString &shortcutText, command.globalShortcuts) {
                QKeySequence shortcut(shortcutText, QKeySequence::PortableText);
                if ( !shortcut.isEmpty() && !usedShortcuts.contains(shortcut) ) {
                    usedShortcuts.append(shortcut);
                    createGlobalShortcut(shortcut, command);
                }
            }
        }
    }
}

void ClipboardServer::onAboutToQuit()
{
    COPYQ_LOG("Closing server.");

    m_wnd->saveTabs();

    if( isMonitoring() )
        stopMonitoring();

    terminateThreads();
}

void ClipboardServer::onCommitData(QSessionManager &sessionManager)
{
    COPYQ_LOG("Got commit data request from session manager.");

    const bool cancel = sessionManager.allowsInteraction() && !askToQuit();
    sessionManager.release();

    if (cancel)
        sessionManager.cancel();
    else
        m_wnd->saveTabs();
}

void ClipboardServer::maybeQuit()
{
    if (askToQuit()) {
        terminateThreads();
        QCoreApplication::exit();
    }
}

bool ClipboardServer::askToQuit()
{
    if ( !m_wnd->maybeCloseCommandDialog() )
        return false;

    if ( m_clientThreads.activeThreadCount() > 0 || m_wnd->hasRunningAction() ) {
        QMessageBox messageBox( QMessageBox::Warning, tr("Cancel Active Commands"),
                                tr("Cancel active commands and exit?"), QMessageBox::NoButton,
                                m_wnd );

        messageBox.addButton(tr("Cancel Exiting"), QMessageBox::RejectRole);
        messageBox.addButton(tr("Exit Anyway"), QMessageBox::AcceptRole);

        messageBox.exec();
        return messageBox.result() == QMessageBox::Accepted;
    }

    return true;
}

void ClipboardServer::terminateThreads()
{
    COPYQ_LOG( QString("Active client threads: %1").arg(m_clientThreads.activeThreadCount()) );

    COPYQ_LOG("Terminating remaining threads.");
    emit terminateClientThreads();
    while ( !m_clientThreads.waitForDone(0) )
        QApplication::processEvents();
}

void ClipboardServer::doCommand(const Arguments &args, ClientSocket *client)
{
    // Worker object without parent needs to be deleted afterwards!
    // There is no parent so as it's possible to move the worker to another thread.
    // QThreadPool takes ownership and worker will be automatically deleted
    // after run() (see QRunnable::setAutoDelete()).
    ScriptableWorker *worker = new ScriptableWorker(m_wnd, args, client, m_itemFactory->scripts());

    // Terminate worker at application exit.
    connect( this, SIGNAL(terminateClientThreads()),
             client, SLOT(close()) );

    // Add client thread to pool.
    m_clientThreads.start(worker);
}

void ClipboardServer::newMonitorMessage(const QByteArray &message)
{
    if ( !m_wnd->isMonitoringEnabled() )
        return;

    QVariantMap data;

    if ( !deserializeData(&data, message) ) {
        log("Failed to read message from monitor.", LogError);
        return;
    }

    m_wnd->clipboardChanged(data);
}

void ClipboardServer::monitorConnectionError()
{
    stopMonitoring();
    startMonitoring();
}

void ClipboardServer::changeClipboard(const QVariantMap &data, QClipboard::Mode mode)
{
    if ( !isMonitoring() ) {
        COPYQ_LOG("Cannot send message to monitor!");
        return;
    }

    COPYQ_LOG("Sending message to monitor.");

    const MonitorMessageCode code =
            mode == QClipboard::Clipboard ? MonitorChangeClipboard : MonitorChangeSelection;

    m_monitor->writeMessage( serializeData(data), code );
}

void ClipboardServer::createGlobalShortcut(const QKeySequence &shortcut, const Command &command)
{
#ifdef NO_GLOBAL_SHORTCUTS
    Q_UNUSED(shortcut);
    Q_UNUSED(command);
#else
    QxtGlobalShortcut *s = new QxtGlobalShortcut(shortcut, this);
    if (!s->isValid()) {
        log(QString("Failed to set global shortcut \"%1\" for command \"%2\".")
            .arg(shortcut.toString())
            .arg(command.name),
            LogWarning);
        delete s;
        return;
    }

    connect( s, SIGNAL(activated(QxtGlobalShortcut*)),
             this, SLOT(shortcutActivated(QxtGlobalShortcut*)) );

    m_shortcutActions[s] = command;
#endif
}

bool ClipboardServer::eventFilter(QObject *object, QEvent *ev)
{
    const QEvent::Type type = ev->type();

    if ( m_ignoreKeysTimer.isActive()
         && (type == QEvent::KeyPress
             || type == QEvent::Shortcut
             || type == QEvent::ShortcutOverride) )
    {
        ev->accept();
        return true;
    }

    // Close menu on Escape key and give focus back to search edit or browser.
    if (type == QEvent::KeyPress) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(ev);
        if (keyevent->key() == Qt::Key_Escape) {
            QMenu *menu = qobject_cast<QMenu*>(object);
            if (menu != NULL) {
                menu->close();
                m_wnd->enterBrowseMode(m_wnd->browseMode());
            }
        }
    } else if (type == QEvent::Paint) {
        setActivePaintDevice(object);
    }

    return false;
}

void ClipboardServer::loadSettings()
{
    // reload clipboard monitor configuration
    if ( isMonitoring() )
        loadMonitorSettings();
}

void ClipboardServer::shortcutActivated(QxtGlobalShortcut *shortcut)
{
    m_ignoreKeysTimer.start();

    const QMap<QxtGlobalShortcut*, Command>::const_iterator it =
            m_shortcutActions.find(shortcut);
    if ( it != m_shortcutActions.end() ) {
        QVariantMap data;
        const QString shortcutText = portableShortcutText(shortcut->shortcut());
        data.insert(mimeShortcut, shortcutText.toUtf8());
        m_wnd->action(data, it.value());
    }
}

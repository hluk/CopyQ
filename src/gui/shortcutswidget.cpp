// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui/shortcutswidget.h"
#include "ui_shortcutswidget.h"

#include "common/command.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/predefinedcommands.h"
#include "common/shortcuts.h"
#include "common/timer.h"
#include "gui/iconfactory.h"
#include "gui/iconfont.h"
#include "gui/icons.h"
#include "gui/menuitems.h"
#include "gui/shortcutbutton.h"

#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QList>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

namespace {

namespace Columns {
enum Columns {
    Icon,
    Text,
    Shortcut,

    Count
};
}

QString uiText(QString text)
{
    return text.remove('&');
}

Command *findShortcutCommand(const QString &name, QVector<Command> &commands)
{
    for (auto &command2 : commands) {
        if (command2.name == name)
            return &command2;
    }

    return nullptr;
}

bool hasShortcutCommand(const QString &name, const QVector<MenuAction> &actions)
{
    for (auto &action : actions) {
        if (action.command.name == name)
            return true;
    }

    return false;
}

bool canAddCommandAction(const Command &command, const QVector<MenuAction> &actions)
{
    return command.type() & (CommandType::Menu | CommandType::GlobalShortcut)
            && !hasShortcutCommand(command.name, actions);
}

const QStringList &shortcuts(const Command &command)
{
    return (command.type() & CommandType::GlobalShortcut)
            ? command.globalShortcuts
            : command.shortcuts;
}

void setShortcuts(Command *command, const QStringList &shortcutNames)
{
    if (command->type() & CommandType::GlobalShortcut)
        command->globalShortcuts = shortcutNames;
    else
        command->shortcuts = shortcutNames;
}

QDBusInterface globalShortcutInterface()
{
    return QDBusInterface(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts")
    );
}

const QString &portalTokenName()
{
    static const QString tokenName = QCoreApplication::applicationName()
        // Use only valid token name characters.
        .replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"));
    return tokenName;
}

QDBusObjectPath initPortalGlobalShortcuts()
{
    qDBusRegisterMetaType<std::pair<QString, QVariantMap>>();
    qDBusRegisterMetaType<QList<QPair<QString, QVariantMap>>>();

    QDBusInterface iface = globalShortcutInterface();
    QDBusMessage message = iface.call(
        QStringLiteral("CreateSession"),
        QMap<QString, QVariant>{
            {QStringLiteral("handle_token"), portalTokenName()},
            {QStringLiteral("session_handle_token"), portalTokenName()}
        }
    );

    if (message.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "Failed to create portal global shortcuts session:"
            << message.errorMessage();
        return {};
    }

    return message.arguments().first().value<QDBusObjectPath>();
}

void configurePortalGlobalShortcuts(const QDBusObjectPath &sessionPath)
{
    QDBusInterface iface = globalShortcutInterface();
    QDBusMessage message = iface.call(
        QStringLiteral("ConfigureShortcuts"),
        sessionPath,
        QString(),
        QMap<QString, QVariant>{
            {QStringLiteral("handle_token"), portalTokenName()}
        }
    );

    if (message.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "Failed to configure portal global shortcuts:"
            << message.errorMessage();
    }
}

bool bindPortalGlobalShortcut(const QDBusObjectPath &sessionPath, const QTableWidget *table)
{
    const auto model = table->model();
    QList<QPair<QString, QVariantMap>> shortcuts;
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, Columns::Text);
        const QString name = model->data(index, Qt::DisplayRole).toString();

        const auto button = qobject_cast<ShortcutButton*>(table->cellWidget(row, Columns::Shortcut));
        Q_ASSERT(button != nullptr);
        if (button == nullptr)
            continue;

        QString preferredTrigger;
        const auto wantShortcuts = button->shortcuts();
        if (!wantShortcuts.isEmpty()) {
            preferredTrigger = wantShortcuts.first()
                .toString(QKeySequence::PortableText)
                .toUpper()
                .replace("SUPER", "LOGO")
                .replace("META", "LOGO");
        }
        qDebug() << "Binding portal global shortcut" << name << "to" << preferredTrigger;
        shortcuts.append({name, {
            {QStringLiteral("description"), name},
            {QStringLiteral("preferred_trigger"), preferredTrigger},
        }});
    }

    QDBusInterface iface = globalShortcutInterface();
    QDBusMessage message = iface.call(
        QStringLiteral("BindShortcuts"),
        sessionPath,
        QVariant::fromValue(shortcuts),
        QString(),
        QMap<QString, QVariant>{
            {QStringLiteral("handle_token"), portalTokenName()}
        }
    );

    if (message.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "Failed to bind portal global shortcuts:"
            << message.errorMessage();
        return false;
    }

    return true;
}

} // namespace

ShortcutsWidget::ShortcutsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ShortcutsWidget)
{
    ui->setupUi(this);

    connect(ui->lineEditFilter, &QLineEdit::textChanged,
            this, &ShortcutsWidget::onLineEditFilterTextChanged);

    const int iconSize = iconFontSizePixels();
    for (auto table : { ui->tableWidgetApplication, ui->tableWidgetGlobal }) {
        table->setColumnCount(Columns::Count);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->hide();
        table->verticalHeader()->hide();
        table->setIconSize(QSize(iconSize, iconSize));
    }

    initSingleShotTimer( &m_timerCheckAmbiguous, 0, this, &ShortcutsWidget::checkAmbiguousShortcuts );

    m_portalSessionHandle = initPortalGlobalShortcuts().path();
    if ( !m_portalSessionHandle.isEmpty() ) {
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            m_portalSessionHandle,
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"),
            this,
            SLOT(onPortalSessionCreated(uint,QVariantMap))
        );
    }
}

ShortcutsWidget::~ShortcutsWidget()
{
    delete ui;
}

void ShortcutsWidget::loadShortcuts(const QSettings &settings)
{
    MenuItems items = menuItems();
    ::loadShortcuts(&items, settings);

    m_actions.clear();
    m_shortcuts.clear();

    for (auto table : { ui->tableWidgetApplication, ui->tableWidgetGlobal }) {
        while (table->rowCount() > 0)
            table->removeRow(0);
    }

    for (const auto &item : items) {
        MenuAction action;
        action.iconName = item.iconName;
        action.iconId = item.iconId;
        action.text = item.text;
        action.settingsKey = item.settingsKey;

        addShortcutRow(action);

        action.shortcutButton->setDefaultShortcut(item.defaultShortcut);
        for (const auto &shortcut : item.shortcuts)
            action.shortcutButton->addShortcut(shortcut);
    }

    addCommands( loadAllCommands() );
    addCommands( predefinedCommands() );
}

void ShortcutsWidget::saveShortcuts(QSettings *settings)
{
    auto commands = loadAllCommands();
    bool needSaveCommands = false;

    for (const auto &action : m_actions) {
        QStringList shortcutNames;
        for (const auto &shortcut : action.shortcutButton->shortcuts())
            shortcutNames.append(portableShortcutText(shortcut));

        if ( action.settingsKey.isEmpty() ) {
            auto savedCommand = findShortcutCommand(action.command.name, commands);
            if (savedCommand) {
                if ( savedCommand->enable ? (shortcuts(*savedCommand) != shortcutNames) : !shortcutNames.empty() ) {
                    needSaveCommands = true;
                    savedCommand->enable = true;
                    setShortcuts(savedCommand, shortcutNames);
                }
            } else if ( !shortcutNames.isEmpty() ) {
                needSaveCommands = true;
                auto command = action.command;
                setShortcuts(&command, shortcutNames);
                commands.append(command);
            }
        } else {
            // Workaround for QTBUG-51237 (saving empty list results in invalid value).
            if (shortcutNames.isEmpty())
                settings->setValue(action.settingsKey, QString());
            else
                settings->setValue(action.settingsKey, shortcutNames);
        }
    }

    if (needSaveCommands) {
        saveCommands(commands);
        emit commandsSaved();
    }
}

void ShortcutsWidget::addCommands(const QVector<Command> &commands)
{
    for ( const auto &command : commands ) {
        if ( canAddCommandAction(command, m_actions) ) {
            MenuAction action;
            action.iconId = toIconId(command.icon);
            action.text = command.name;
            action.command = command;
            addShortcutRow(action);

            if (command.enable) {
                for (const auto &shortcut : shortcuts(command))
                    action.shortcutButton->addShortcut(shortcut);
            }
        }
    }
}

void ShortcutsWidget::showEvent(QShowEvent *event)
{
    for (auto &action : m_actions) {
        if ( action.tableItem->icon().isNull() )
            action.tableItem->setIcon( getIcon(action.iconName, action.iconId) );
    }

    QWidget::showEvent(event);

    for (auto table : { ui->tableWidgetApplication, ui->tableWidgetGlobal }) {
        table->resizeColumnToContents(Columns::Icon);
        table->resizeColumnToContents(Columns::Text);
    }
    m_timerCheckAmbiguous.start(); // Update because shortcuts for commands may have changed.
}

void ShortcutsWidget::onShortcutAdded(const QKeySequence &shortcut)
{
    m_shortcuts.append(shortcut);
    m_timerCheckAmbiguous.start();
}

void ShortcutsWidget::onShortcutRemoved(const QKeySequence &shortcut)
{
    m_shortcuts.removeOne(shortcut);
    m_timerCheckAmbiguous.start();
}

void ShortcutsWidget::checkAmbiguousShortcuts()
{
    const auto iconAmbiguous = getIcon("", IconCircleExclamation);
    const auto toolTipAmbiguous = tr("Shortcut already exists!");

    std::sort( m_shortcuts.begin(), m_shortcuts.end() );
    QList<QKeySequence> ambiguousShortcuts;
    for ( int i = 1; i < m_shortcuts.size(); ++i ) {
        if (m_shortcuts[i] == m_shortcuts[i - 1])
            ambiguousShortcuts.append(m_shortcuts[i]);
    }

    for ( const auto &action : m_actions )
        action.shortcutButton->checkAmbiguousShortcuts(ambiguousShortcuts, iconAmbiguous, toolTipAmbiguous);
}

void ShortcutsWidget::onLineEditFilterTextChanged(const QString &text)
{
    const QString needle = text.toLower();

    for ( const auto &action : m_actions ) {
        bool found = uiText(action.text).toLower().contains(needle);
        if (!found) {
                for ( const auto &shortcut : action.shortcutButton->shortcuts() ) {
                    if ( shortcut.toString(QKeySequence::NativeText).toLower().contains(needle) ) {
                        found = true;
                        break;
                    }
                }
        }

        const int row = action.tableItem->row();
        QTableWidget *table = action.tableItem->tableWidget();
        table->setRowHidden(row, !found);
    }
}

void ShortcutsWidget::addShortcutRow(MenuAction &action)
{
    const bool isGlobal = action.command.type() & CommandType::GlobalShortcut;

    QTableWidget *table = isGlobal
        ? ui->tableWidgetGlobal
        : ui->tableWidgetApplication;

    const int row = table->rowCount();
    table->insertRow(row);

    auto tableItem = new QTableWidgetItem();
    action.tableItem = tableItem;
    table->setItem(row, Columns::Icon, tableItem);
    tableItem->setFlags(Qt::ItemIsEnabled);

    tableItem = new QTableWidgetItem(uiText(action.text));
    table->setItem(row, Columns::Text, tableItem);
    tableItem->setFlags(Qt::ItemIsEnabled);

    action.shortcutButton = new ShortcutButton(table);
    table->setCellWidget(row, Columns::Shortcut, action.shortcutButton);

    m_actions.append(action);

    connect( action.shortcutButton, &ShortcutButton::shortcutAdded,
             this, &ShortcutsWidget::onShortcutAdded );
    connect( action.shortcutButton, &ShortcutButton::shortcutRemoved,
             this, &ShortcutsWidget::onShortcutRemoved );
}

void ShortcutsWidget::onPortalSessionCreated(uint responseCode, const QVariantMap& results)
{
    if (responseCode != 0) {
        qWarning() << "Failed to create portal global shortcuts session:" << results;
        return;
    } 

    const QDBusObjectPath sessionPath(
        results.value(QStringLiteral("session_handle")).value<QString>());
    if (sessionPath.path().isEmpty()) {
        qWarning() << "Failed to get portal global shortcuts session path:" << results;
        return;
    }

    qDebug() << "Portal global shortcut session: " << sessionPath.path();

    QDBusConnection::sessionBus().disconnect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        m_portalSessionHandle,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(onPortalSessionCreated(uint,QVariantMap))
    );

    QSettings settings;
    if ( settings.value(QStringLiteral("use_portal_global_shortcuts")).toBool() ) {
        bindPortalGlobalShortcut(sessionPath, ui->tableWidgetGlobal);
    }

    auto button = new QPushButton(
        tr("Configure Shortcuts in Desktop Environment"), ui->tabGlobal);
    ui->tabGlobal->layout()->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, sessionPath]{
        if ( bindPortalGlobalShortcut(sessionPath, ui->tableWidgetGlobal) ) {
            QSettings settings;
            settings.setValue(QStringLiteral("use_portal_global_shortcuts"), true);
            configurePortalGlobalShortcuts(sessionPath);
        }
    });

    QObject *server = qApp->findChild<QObject*>(
        QStringLiteral("CopyQClipboardServer"), Qt::FindDirectChildrenOnly);
    Q_ASSERT(server != nullptr);
    if (server == nullptr)
        return;

    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        QStringLiteral("Activated"),
        server,
        SLOT(onPortalGlobalShortcutActivated(QDBusObjectPath,QString,qulonglong,QVariantMap))
    );
}

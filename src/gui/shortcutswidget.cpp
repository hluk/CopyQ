/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include "gui/shortcutswidget.h"
#include "ui_shortcutswidget.h"

#include "common/command.h"
#include "common/common.h"
#include "common/predefinedcommands.h"
#include "common/shortcuts.h"
#include "gui/commanddialog.h"
#include "gui/iconfactory.h"
#include "gui/iconfont.h"
#include "gui/icons.h"
#include "gui/menuitems.h"
#include "gui/shortcutbutton.h"

#include <QList>
#include <QPushButton>
#include <QSettings>

#include <algorithm>

namespace {

namespace Columns {
enum Columns {
    Empty, // First column is disabled so the widgets are focused instead of table on Tab key.
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

} // namespace

ShortcutsWidget::ShortcutsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ShortcutsWidget)
{
    ui->setupUi(this);

    ui->tableWidget->setColumnCount(Columns::Count);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->horizontalHeader()->hide();
    ui->tableWidget->horizontalHeader()->resizeSection(Columns::Empty, 0);
    ui->tableWidget->verticalHeader()->hide();

    const int iconSize = iconFontSizePixels();
    ui->tableWidget->setIconSize(QSize(iconSize, iconSize));

    initSingleShotTimer( &m_timerCheckAmbiguous, 0, this, SLOT(checkAmbiguousShortcuts()) );
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

    QTableWidget *table = ui->tableWidget;
    while (table->rowCount() > 0)
        table->removeRow(0);

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

    addCommands( loadEnabledCommands() );
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
            const bool enable = !shortcutNames.isEmpty();
            if (savedCommand) {
                if ( savedCommand->enable != enable || (enable && savedCommand->globalShortcuts != shortcutNames) ) {
                    needSaveCommands = true;
                    savedCommand->enable = enable;
                    savedCommand->globalShortcuts = shortcutNames;
                }
            } else if ( !shortcutNames.isEmpty() ) {
                needSaveCommands = true;
                auto command = action.command;
                command.globalShortcuts = shortcutNames;
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

            for (const auto &shortcut : command.globalShortcuts)
                action.shortcutButton->addShortcut(shortcut);
            for (const auto &shortcut : command.shortcuts)
                action.shortcutButton->addShortcut(shortcut);
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
    ui->tableWidget->resizeColumnToContents(Columns::Text);
    ui->tableWidget->resizeColumnToContents(Columns::Icon);
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
    const auto iconAmbiguous = getIcon("", IconExclamationCircle);
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

void ShortcutsWidget::on_lineEditFilter_textChanged(const QString &text)
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
        if (found)
            ui->tableWidget->showRow(row);
        else
            ui->tableWidget->hideRow(row);
    }
}

void ShortcutsWidget::addShortcutRow(MenuAction &action)
{
    QTableWidget *table = ui->tableWidget;

    const int row = table->rowCount();
    table->insertRow(row);

    auto tableItem = new QTableWidgetItem();
    table->setItem(row, Columns::Empty, tableItem);
    tableItem->setFlags(Qt::NoItemFlags);

    tableItem = new QTableWidgetItem();
    action.tableItem = tableItem;
    table->setItem(row, Columns::Icon, tableItem);
    tableItem->setFlags(Qt::ItemIsEnabled);

    tableItem = new QTableWidgetItem(uiText(action.text));
    table->setItem(row, Columns::Text, tableItem);
    tableItem->setFlags(Qt::ItemIsEnabled);

    action.shortcutButton = new ShortcutButton(table);
    table->setCellWidget(row, Columns::Shortcut, action.shortcutButton);

    m_actions.append(action);

    connect( action.shortcutButton, SIGNAL(shortcutAdded(QKeySequence)),
             this, SLOT(onShortcutAdded(QKeySequence)) );
    connect( action.shortcutButton, SIGNAL(shortcutRemoved(QKeySequence)),
             this, SLOT(onShortcutRemoved(QKeySequence)) );
}

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

#include "gui/shortcutswidget.h"
#include "ui_shortcutswidget.h"

#include "common/command.h"
#include "common/common.h"
#include "gui/commanddialog.h"
#include "gui/iconfactory.h"
#include "gui/iconfont.h"
#include "gui/icons.h"
#include "gui/menuitems.h"
#include "gui/shortcutbutton.h"

#include <QList>
#include <QPointer>
#include <QPushButton>
#include <QSettings>

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

void ShortcutsWidget::loadShortcuts(QSettings &settings)
{
    MenuItems items = menuItems();
    ::loadShortcuts(&items, settings);

    m_actions.clear();
    m_shortcuts.clear();

    QTableWidget *table = ui->tableWidget;
    while (table->rowCount() > 0)
        table->removeRow(0);


    foreach (const MenuItem &item, items) {
        MenuAction action;
        action.iconName = item.iconName;
        action.iconId = item.iconId;
        action.text = item.text;
        action.settingsKey = item.settingsKey;

        const int row = table->rowCount();
        table->insertRow(row);

        QTableWidgetItem *tableItem = new QTableWidgetItem();
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
        action.shortcutButton->setDefaultShortcut(item.defaultShortcut);
        foreach (const QKeySequence &shortcut, item.shortcuts)
            action.shortcutButton->addShortcut(shortcut);

        action.iconId = item.iconId;
        m_actions.append(action);

        m_shortcuts << item.shortcuts;
        m_timerCheckAmbiguous.start();

        connect( action.shortcutButton, SIGNAL(shortcutAdded(QKeySequence)),
                 this, SLOT(onShortcutAdded(QKeySequence)) );
        connect( action.shortcutButton, SIGNAL(shortcutRemoved(QKeySequence)),
                 this, SLOT(onShortcutRemoved(QKeySequence)) );
    }
}

void ShortcutsWidget::saveShortcuts(QSettings &settings) const
{
    foreach (const MenuAction &action, m_actions) {
        if ( action.settingsKey.isEmpty() ) {
            QStringList shortcutNames;
            foreach (const QKeySequence &shortcut, action.shortcutButton->shortcuts())
                shortcutNames.append(portableShortcutText(shortcut));
            settings.setValue(action.settingsKey, shortcutNames);
        }
    }
}

void ShortcutsWidget::showEvent(QShowEvent *event)
{
    for (int i = 0; i < m_actions.size(); ++i) {
        MenuAction &action = m_actions[i];
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
    static const QIcon iconOverriden = getIcon("", IconInfoSign);
    static const QIcon iconAmbiguous = getIcon("", IconExclamationSign);
    static const QString toolTipOverriden = tr("There is command overriding this shortcut.");
    static const QString toolTipAmbiguous = tr("Shortcut already exists!");

    qSort(m_shortcuts);
    QList<QKeySequence> ambiguousShortcuts;
    for ( int i = 1; i < m_shortcuts.size(); ++i ) {
        if (m_shortcuts[i] == m_shortcuts[i - 1])
            ambiguousShortcuts.append(m_shortcuts[i]);
    }

    QList<QKeySequence> commandShortcuts;
    foreach ( const Command &command, loadCommands(true) ) {
        foreach (const QString &shortcutText, command.shortcuts + command.globalShortcuts) {
            const QKeySequence shortcut(shortcutText, QKeySequence::PortableText);
            if ( !shortcut.isEmpty() )
                commandShortcuts.append(shortcut);
        }
    }

    foreach ( const MenuAction &action, m_actions ) {
        action.shortcutButton->checkAmbiguousShortcuts(commandShortcuts, iconOverriden, toolTipOverriden);
        action.shortcutButton->checkAmbiguousShortcuts(ambiguousShortcuts, iconAmbiguous, toolTipAmbiguous);
    }
}

void ShortcutsWidget::on_lineEditFilter_textChanged(const QString &text)
{
    const QString needle = text.toLower();

    foreach ( const MenuAction &action, m_actions ) {
        bool found = uiText(action.text).toLower().contains(needle);
        if (!found) {
                foreach ( const QKeySequence &shortcut, action.shortcutButton->shortcuts() ) {
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

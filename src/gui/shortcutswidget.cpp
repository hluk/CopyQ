/*
    Copyright (c) 2015, Lukas Holecek <hluk@email.cz>

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
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
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

} // namespace

class MenuAction : public QObject {
    Q_OBJECT

public:
    MenuAction(
            const QString &text, const QKeySequence &shortcut, const QString &settingsKey,
            QTableWidget *table, const QString &iconName, ushort iconId,
            const QList<QKeySequence> &disabledShortcuts)
        : QObject()
        , m_iconName(iconName)
        , m_iconId(iconId)
        , m_text(text)
        , m_settingsKey(settingsKey)
        , m_tableItem(NULL)
        , m_shortcutButton(new ShortcutButton(table))
        , m_action()
        , m_disabledShortcuts(disabledShortcuts)
    {
        const int row = table->rowCount();
        table->insertRow(row);

        QTableWidgetItem *tableItem = new QTableWidgetItem();
        table->setItem(row, Columns::Empty, tableItem);
        tableItem->setFlags(Qt::NoItemFlags);

        m_tableItem = new QTableWidgetItem();
        table->setItem(row, Columns::Icon, m_tableItem);
        m_tableItem->setFlags(Qt::ItemIsEnabled);

        tableItem = new QTableWidgetItem(uiText());
        table->setItem(row, Columns::Text, tableItem);
        tableItem->setFlags(Qt::ItemIsEnabled);

        table->setCellWidget(row, Columns::Shortcut, m_shortcutButton);

        m_shortcutButton->setDefaultShortcut(shortcut);
        m_shortcutButton->installEventFilter(this);

        connect( m_shortcutButton, SIGNAL(shortcutAdded(QKeySequence)),
                 this, SLOT(onShortcutAdded(QKeySequence)) );
        connect( m_shortcutButton, SIGNAL(shortcutRemoved(QKeySequence)),
                 this, SLOT(onShortcutRemoved(QKeySequence)) );
    }

    ~MenuAction()
    {
        delete m_action;
    }

    void updateIcons()
    {
        if ( m_tableItem->icon().isNull() )
            m_tableItem->setIcon( getIcon(m_iconName, m_iconId) );
    }

    void loadShortcuts(QSettings &settings)
    {
        if ( m_settingsKey.isEmpty() )
            return;

        QVariant shortcutNames = settings.value(m_settingsKey);
        if ( shortcutNames.isValid() ) {
            m_shortcutButton->clearShortcuts();
            foreach ( const QString &shortcut, shortcutNames.toStringList() )
                m_shortcutButton->addShortcut(shortcut);
        } else {
            m_shortcutButton->resetShortcuts();
        }

        updateActionShortcuts();
    }

    void saveShortcuts(QSettings &settings) const
    {
        if ( m_settingsKey.isEmpty() )
            return;

        QStringList shortcutNames;
        foreach (const QKeySequence &shortcut, shortcuts() )
            shortcutNames.append( shortcut.toString(QKeySequence::PortableText) );
        settings.setValue(m_settingsKey, shortcutNames);
    }

    QAction *action(QWidget *parent, Qt::ShortcutContext context)
    {
        if ( m_action.isNull() ) {
            m_action = new QAction(m_text, NULL);
            m_action->setIcon( getIcon(m_iconName, m_iconId) );
            updateActionShortcuts();
        }

        if (parent != NULL)
            parent->addAction(m_action);

        if (parent != NULL)
            m_action->setShortcutContext(context);
        return m_action;
    }

    void checkAmbiguousShortcuts(const QList<QKeySequence> &ambiguousShortcuts,
                                 const QIcon &warningIcon, const QString &warningToolTip) const
    {
        m_shortcutButton->checkAmbiguousShortcuts(ambiguousShortcuts, warningIcon, warningToolTip);
    }

    QList<QKeySequence> shortcuts() const { return m_shortcutButton->shortcuts(); }

    QString uiText() const
    {
        QString label = m_text;
        label.remove('&');
        return label;
    }

    int tableRow() const
    {
        return m_tableItem->row();
    }

    QWidget *shortcutButton() { return m_shortcutButton; }

    void updateActionShortcuts()
    {
        if ( m_action.isNull() )
            return;

        QList<QKeySequence> enabledShortcuts;
        foreach (const QKeySequence& shortcut, shortcuts()) {
            if ( !m_disabledShortcuts.contains(shortcut) )
                enabledShortcuts.append(shortcut);
        }
        m_action->setShortcuts(enabledShortcuts);
    }

signals:
    void shortcutAdded(const QKeySequence &shortcut);
    void shortcutRemoved(const QKeySequence &shortcut);

private slots:
    void onShortcutAdded(const QKeySequence &shortcut)
    {
        if ( !m_action.isNull() )
            updateActionShortcuts();
        emit shortcutAdded(shortcut);
    }

    void onShortcutRemoved(const QKeySequence &shortcut)
    {
        if ( !m_action.isNull() )
            updateActionShortcuts();
        emit shortcutRemoved(shortcut);
    }

private:
    QString m_iconName;
    ushort m_iconId;
    QString m_text;
    QString m_settingsKey;
    QTableWidgetItem *m_tableItem;
    ShortcutButton *m_shortcutButton;
    QPointer<QAction> m_action;
    const QList<QKeySequence> &m_disabledShortcuts;
};

ShortcutsWidget::ShortcutsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ShortcutsWidget)
{
    ui->setupUi(this);

    ui->tableWidget->setColumnCount(Columns::Count);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->horizontalHeader()->hide();
    ui->tableWidget->horizontalHeader()->resizeSection(Columns::Empty, 0);
    ui->tableWidget->horizontalHeader()->resizeSection(Columns::Icon, 24);
    ui->tableWidget->verticalHeader()->hide();

    initSingleShotTimer( &m_timerCheckAmbiguous, 0, this, SLOT(checkAmbiguousShortcuts()) );
}

ShortcutsWidget::~ShortcutsWidget()
{
    delete ui;
}

void ShortcutsWidget::loadShortcuts(QSettings &settings)
{
    foreach (const MenuActionPtr &action, m_actions) {
        action->loadShortcuts(settings);
    }
}

void ShortcutsWidget::saveShortcuts(QSettings &settings) const
{
    foreach (const MenuActionPtr &action, m_actions) {
        action->saveShortcuts(settings);
    }
}

void ShortcutsWidget::addAction(
        int id, const QString &text, const QString &settingsKey, const QKeySequence &shortcut,
        const QString &iconName, ushort iconId)
{
    Q_ASSERT(!hasAction(id));

    MenuAction *action = new MenuAction(
                text, shortcut, settingsKey, ui->tableWidget, iconName, iconId, m_disabledShortcuts);
    m_actions.insert( id, MenuActionPtr(action) );
    m_shortcuts << action->shortcuts();
    m_timerCheckAmbiguous.start();

    connect( action, SIGNAL(shortcutAdded(QKeySequence)),
             this, SLOT(onShortcutAdded(QKeySequence)) );
    connect( action, SIGNAL(shortcutRemoved(QKeySequence)),
             this, SLOT(onShortcutRemoved(QKeySequence)) );
}

void ShortcutsWidget::addAction(
        int id, const QString &text, const QString &settingsKey, const QString &shortcutNativeText,
        const QString &iconName, ushort iconId)
{
    const QKeySequence shortcut(shortcutNativeText, QKeySequence::NativeText);
    addAction(id, text, settingsKey, shortcut, iconName, iconId);
}

QAction *ShortcutsWidget::action(int id, QWidget *parent, Qt::ShortcutContext context)
{
    return action(id)->action(parent, context);
}

QList<QKeySequence> ShortcutsWidget::shortcuts(int id) const
{
    return action(id)->shortcuts();
}

void ShortcutsWidget::setDisabledShortcuts(const QList<QKeySequence> &shortcuts)
{
    m_disabledShortcuts = shortcuts;
    foreach ( const MenuActionPtr &action, m_actions )
        action->updateActionShortcuts();
}

const QList<QKeySequence> &ShortcutsWidget::disabledShortcuts() const
{
    return m_disabledShortcuts;
}

void ShortcutsWidget::showEvent(QShowEvent *event)
{
    foreach ( const MenuActionPtr &menuAction, m_actions.values() )
        menuAction->updateIcons();

    QWidget::showEvent(event);
    ui->tableWidget->resizeColumnToContents(Columns::Text);
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

    foreach ( const MenuActionPtr &action, m_actions ) {
        action->checkAmbiguousShortcuts(commandShortcuts, iconOverriden, toolTipOverriden);
        action->checkAmbiguousShortcuts(ambiguousShortcuts, iconAmbiguous, toolTipAmbiguous);
    }
}

void ShortcutsWidget::on_lineEditFilter_textChanged(const QString &text)
{
    const QString needle = text.toLower();

    foreach ( const MenuActionPtr &action, m_actions ) {
        bool found = action->uiText().toLower().contains(needle);
        if (!found) {
                foreach ( const QKeySequence &shortcut, action->shortcuts() ) {
                    if ( shortcut.toString(QKeySequence::NativeText).toLower().contains(needle) ) {
                        found = true;
                        break;
                    }
                }
        }

        const int row = action->tableRow();
        if (found)
            ui->tableWidget->showRow(row);
        else
            ui->tableWidget->hideRow(row);
    }
}

MenuAction *ShortcutsWidget::action(int id) const
{
    Q_ASSERT(hasAction(id));
    return m_actions[id].data();
}

#include "shortcutswidget.moc"

/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
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

int normalize(int value)
{
    return qMin(255, qMax(0, value));
}

QColor tintColor(const QColor &c, int r, int g, int b, int a = 0)
{
    return QColor(
                normalize(c.red() + r),
                normalize(c.green() + g),
                normalize(c.blue() + b),
                normalize(c.alpha() + a)
                );
}

} // namespace

class MenuAction : public QObject {
    Q_OBJECT

public:
    MenuAction(const QString &text, const QKeySequence &shortcut, const QString &settingsKey,
               QTableWidget *table)
        : QObject()
        , m_icon()
        , m_text(text)
        , m_settingsKey(settingsKey)
        , m_tableItem(NULL)
        , m_shortcutButton(new ShortcutButton(shortcut, table))
        , m_action()
        , m_disabledShortcuts()
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

        updateAction();
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
            m_action = new QAction(m_icon, m_text, NULL);
            m_action->setShortcuts(shortcuts());
        }

        if (parent != NULL)
            parent->addAction(m_action);

        if (parent != NULL)
            m_action->setShortcutContext(context);
        return m_action;
    }

    void updateIcons(const QIcon &icon)
    {
        if ( !icon.isNull() )
            m_icon = icon;
        m_tableItem->setIcon(m_icon);
        m_shortcutButton->updateIcons();
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

    void setDisabledShortcuts(const QList<QKeySequence> &shortcuts)
    {
        m_disabledShortcuts = shortcuts;
        updateAction();
    }

    QWidget *shortcutButton() { return m_shortcutButton; }

signals:
    void shortcutAdded(const QKeySequence &shortcut);
    void shortcutRemoved(const QKeySequence &shortcut);

private slots:
    void onShortcutAdded(const QKeySequence &shortcut)
    {
        if ( !m_action.isNull() )
            m_action->setShortcuts(shortcuts());
        emit shortcutAdded(shortcut);
    }

    void onShortcutRemoved(const QKeySequence &shortcut)
    {
        if ( !m_action.isNull() )
            m_action->setShortcuts(shortcuts());
        emit shortcutRemoved(shortcut);
    }

private:
    void updateAction()
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

    QIcon m_icon;
    QString m_text;
    QString m_settingsKey;
    QTableWidgetItem *m_tableItem;
    ShortcutButton *m_shortcutButton;
    QPointer<QAction> m_action;
    QList<QKeySequence> m_disabledShortcuts;
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

    m_timerCheckAmbiguous.setSingleShot(true);
    m_timerCheckAmbiguous.setInterval(0);
    connect( &m_timerCheckAmbiguous, SIGNAL(timeout()),
             this, SLOT(checkAmbiguousShortcuts()) );
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

void ShortcutsWidget::addAction(int id, const QString &text, const QKeySequence &shortcut, const QString &settingsKey)
{
    Q_ASSERT(!hasAction(id));

    MenuAction *action = new MenuAction(text, shortcut, settingsKey, ui->tableWidget);
    m_actions.insert( id, QSharedPointer<MenuAction>(action) );
    m_shortcuts << action->shortcuts();
    m_timerCheckAmbiguous.start();

    ui->tableWidget->resizeColumnToContents(Columns::Text);

    connect( action, SIGNAL(shortcutAdded(QKeySequence)),
             this, SLOT(onShortcutAdded(QKeySequence)) );
    connect( action, SIGNAL(shortcutRemoved(QKeySequence)),
             this, SLOT(onShortcutRemoved(QKeySequence)) );
}

QAction *ShortcutsWidget::action(int id, QWidget *parent, Qt::ShortcutContext context)
{
    return action(id)->action(parent, context);
}

QList<QKeySequence> ShortcutsWidget::shortcuts(int id) const
{
    return action(id)->shortcuts();
}

void ShortcutsWidget::updateIcons(int id, const QIcon &icon)
{
    action(id)->updateIcons(icon);
}

void ShortcutsWidget::setDisabledShortcuts(const QList<QKeySequence> &shortcuts)
{
    foreach ( const MenuActionPtr &action, m_actions )
        action->setDisabledShortcuts(shortcuts);
}

void ShortcutsWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
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
    static const QColor c = getDefaultIconColor<QPushButton>(QPalette::Window);
    static const QColor colorOverriden = tintColor(c, -40, -40, 80, -100);
    static const QColor colorAmbiguous = tintColor(c, 80, -40, -40);
    static const QIcon iconOverriden = getIcon("", IconInfoSign, colorOverriden, colorOverriden);
    static const QIcon iconAmbiguous = getIcon("", IconExclamationSign, colorAmbiguous, colorAmbiguous);
    static const QString toolTipOverriden = tr("There is command overriding this shortcut.");
    static const QString toolTipAmbiguous = tr("Shortcut already exists!");

    qSort(m_shortcuts);
    QList<QKeySequence> ambiguousShortcuts;
    for ( int i = 1; i < m_shortcuts.size(); ++i ) {
        if (m_shortcuts[i] == m_shortcuts[i - 1])
            ambiguousShortcuts.append(m_shortcuts[i]);
    }

    QList<QKeySequence> commandShortcuts;
    foreach ( const Command &command, ConfigurationManager::instance()->commands(true, false) ) {
        if ( !command.shortcut.isEmpty() )
            commandShortcuts.append(command.shortcut);
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

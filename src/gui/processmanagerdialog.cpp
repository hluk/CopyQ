/*
    Copyright (c) 2019, Lukas Holecek <hluk@email.cz>

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

#include "gui/processmanagerdialog.h"
#include "ui_processmanagerdialog.h"

#include "common/action.h"
#include "common/shortcuts.h"
#include "gui/iconfont.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"

#include <QAction>
#include <QDateTime>
#include <QToolButton>

#include <algorithm>
#include <functional>

namespace {

const int maxNumberOfProcesses = 100;
const auto dateTimeFormat = "yyyy-MM-dd HH:mm:ss.zzz";

namespace statusItemData {
enum {
    actionId = Qt::UserRole,
    status
};
}

namespace tableCommandsColumns {
enum {
    action,
    name,
    status,
    beginTime,
    endTime,
    count
};
}

QString columnText(int column)
{
    switch (column) {
    case tableCommandsColumns::action:
        return QString();
    case tableCommandsColumns::beginTime:
        return ProcessManagerDialog::tr("Started");
    case tableCommandsColumns::endTime:
        return ProcessManagerDialog::tr("Finished");
    case tableCommandsColumns::name:
        return ProcessManagerDialog::tr("Name");
    case tableCommandsColumns::status:
        return ProcessManagerDialog::tr("Status");
    default:
        Q_ASSERT(false && "Undefined name for column!");
    }

    return QString();
}

QString currentTime()
{
    return QDateTime::currentDateTime().toString(dateTimeFormat);
}

class SortingGuard {
public:
    explicit SortingGuard(QTableWidget *table)
        : m_table(table)
    {
        if ( m_table->isSortingEnabled() )
            m_table->setSortingEnabled(false);
        else
            m_table = nullptr;
    }

    ~SortingGuard()
    {
        if (m_table)
            m_table->setSortingEnabled(true);
    }
private:
    Q_DISABLE_COPY(SortingGuard)

    QTableWidget *m_table;
};

quintptr actionId(const Action *act)
{
    return reinterpret_cast<quintptr>(act);
}

} // namespace

class ProcessManagerDialog::TableRow {
public:
    TableRow(int row, QTableWidget *table)
        : m_table(table)
        , m_item( m_table->item(row, tableCommandsColumns::status) )
    {
    }

    QTableWidgetItem *item(int column)
    {
        return m_table->item(row(), column);
    }

    QToolButton *button(int column)
    {
        QWidget *cellWidget = m_table->cellWidget(row(), column);
        return qobject_cast<QToolButton*>(cellWidget);
    }

    void remove()
    {
        m_table->removeRow( row() );
    }

private:
    int row() const {
        Q_ASSERT(m_item);
        return m_item->row();
    }

    QTableWidget *m_table = nullptr;
    QTableWidgetItem *m_item = nullptr;
};

ProcessManagerDialog::ProcessManagerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ProcessManagerDialog)
{
    ui->setupUi(this);

    WindowGeometryGuard::create(this);

    QTableWidget *t = ui->tableWidgetCommands;
    t->setColumnCount(tableCommandsColumns::count);
    for (int i = 0; i < tableCommandsColumns::count; ++i)
        t->setHorizontalHeaderItem(i, new QTableWidgetItem(columnText(i)) );

    auto act = new QAction(this);
    act->setShortcut(shortcutToRemove());
    connect( act, &QAction::triggered,
             this, &ProcessManagerDialog::onDeleteShortcut );
    addAction(act);
}

ProcessManagerDialog::~ProcessManagerDialog()
{
    delete ui;
}

void ProcessManagerDialog::actionAboutToStart(Action *action)
{
    const QString name = action->name();
    const QString command = action->commandLine();

    auto tableRow = createTableRow( name.isEmpty() ? command : name, action );

    tableRow.item(tableCommandsColumns::name)->setToolTip(command);

    const auto id = actionId(action);
    QTableWidgetItem *statusItem = tableRow.item(tableCommandsColumns::status);
    statusItem->setData(statusItemData::actionId, id);
    statusItem->setData(statusItemData::status, QProcess::Starting);

    // Limit rows in table.
    QTableWidget *t = ui->tableWidgetCommands;
    if (t->rowCount() > maxNumberOfProcesses) {
        for ( int row = t->rowCount() - 1;
              row >= 0 && (!removeIfNotRunning(row) || t->rowCount() > maxNumberOfProcesses);
              --row )
        {
        }
    }
}

void ProcessManagerDialog::actionStarted(Action *action)
{
    auto tableRow = tableRowForAction(action);
    QTableWidgetItem *statusItem = tableRow.item(tableCommandsColumns::status);
    statusItem->setText(tr("Running"));
    statusItem->setData(statusItemData::status, QProcess::Running);
    updateTable();
}

void ProcessManagerDialog::actionFinished(Action *action)
{
    const auto endTime = QDateTime::currentDateTime();

    auto tableRow = tableRowForAction(action);

    const QString status = action->actionFailed() ? tr("Failed") : tr("Finished");

    QTableWidget *t = ui->tableWidgetCommands;
    SortingGuard sortGuard(t);

    QToolButton *button = tableRow.button(tableCommandsColumns::action);
    QTableWidgetItem *statusItem = tableRow.item(tableCommandsColumns::status);
    statusItem->setText(status);
    statusItem->setData(statusItemData::status, QProcess::NotRunning);

    const auto startTime = QDateTime::fromString( tableRow.item(tableCommandsColumns::beginTime)->text(), dateTimeFormat );
    const auto msecs = startTime.msecsTo(endTime);
    const auto seconds = msecs / 1000;
    const auto minutes = seconds / 60;
    const auto hours = minutes / 60;
    const auto days = hours / 24;
    auto endTimeText = QString("%1:%2:%3.%4")
            .arg(hours % 24, 2, 10, QLatin1Char('0'))
            .arg(minutes % 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'))
            .arg(msecs % 1000, 3, 10, QLatin1Char('0'));
    if (days)
        endTimeText.prepend( QString("%1d ").arg(days) );
    tableRow.item(tableCommandsColumns::endTime)->setText(endTimeText);

    if (button) {
        button->setToolTip( tr("Remove") );
        button->setProperty( "text", QString(IconTrash) );
        updateTable();

        button->disconnect();
        connect( button, &QToolButton::clicked,
                 this, &ProcessManagerDialog::onRemoveActionButtonClicked );
    }

    // Reset action ID so it can be used again.
    tableRow.item(tableCommandsColumns::status)->setData(statusItemData::actionId, 0);
}

void ProcessManagerDialog::actionFinished(const QString &name)
{
    createTableRow(name);
}

void ProcessManagerDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    updateTable();
}

void ProcessManagerDialog::onRemoveActionButtonClicked()
{
    Q_ASSERT(sender());
    auto tableRow = tableRowForActionButton(sender());
    tableRow.remove();
}

void ProcessManagerDialog::onDeleteShortcut()
{
    const QList<QTableWidgetItem *> selectedItems = ui->tableWidgetCommands->selectedItems();

    QVector<int> rows;

    rows.reserve( selectedItems.size() );
    for (auto item : selectedItems)
        rows.append( ui->tableWidgetCommands->row(item) );

    std::sort( rows.begin(), rows.end(), std::greater<int>() );

    int lastRow = -1;
    for (int row : rows) {
        if (lastRow != row) {
            removeIfNotRunning(row);
            lastRow = row;
        }
    }
}

ProcessManagerDialog::TableRow ProcessManagerDialog::tableRowForAction(Action *action) const
{
    QTableWidget *t = ui->tableWidgetCommands;
    const auto id = actionId(action);
    for (int row = 0; row < t->rowCount(); ++row) {
        const auto item = t->item(row, tableCommandsColumns::status);
        if ( item->data(statusItemData::actionId) == id )
            return TableRow(row, t);
    }

    Q_ASSERT(false);
    return TableRow(0, t);
}

ProcessManagerDialog::TableRow ProcessManagerDialog::tableRowForActionButton(QObject *button) const
{
    QTableWidget *t = ui->tableWidgetCommands;
    for (int row = 0; row < t->rowCount(); ++row) {
        if ( t->cellWidget(row, tableCommandsColumns::action) == button )
            return TableRow(row, t);
    }

    Q_ASSERT(false);
    return TableRow(0, t);
}

bool ProcessManagerDialog::removeIfNotRunning(int row)
{
    QTableWidget *t = ui->tableWidgetCommands;
    if ( t->item(row, tableCommandsColumns::status)->data(statusItemData::status) != QProcess::NotRunning )
        return false;

    t->removeRow(row);
    return true;
}

void ProcessManagerDialog::updateTable()
{
    if (isVisible())
        ui->tableWidgetCommands->resizeColumnsToContents();
}

ProcessManagerDialog::TableRow ProcessManagerDialog::createTableRow(const QString &name, Action *action)
{
    QTableWidget *t = ui->tableWidgetCommands;
    SortingGuard sortGuard(t);

    const QString status = action ? tr("Starting") : tr("Finished");

    t->insertRow(0);
    t->setItem( 0, tableCommandsColumns::name, new QTableWidgetItem(name) );
    t->setItem( 0, tableCommandsColumns::status, new QTableWidgetItem(status) );
    t->setItem( 0, tableCommandsColumns::beginTime, new QTableWidgetItem(currentTime()) );
    t->setItem( 0, tableCommandsColumns::endTime, new QTableWidgetItem(action ? "-" : "00:00:00.000") );
    t->setCellWidget( 0, tableCommandsColumns::action, createRemoveButton(action) );
    updateTable();

    return TableRow(0, t);
}

QWidget *ProcessManagerDialog::createRemoveButton(Action *action)
{
    QToolButton *button = new QToolButton(ui->tableWidgetCommands);

    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setText( action ? QString(IconBan) : QString(IconTrash) );
    button->setFont(iconFont());
    button->setToolTip(tr("Terminate"));

    if (action) {
        connect( button, &QAbstractButton::clicked,
                 action, &Action::terminate );
    } else {
        connect( button, &QAbstractButton::clicked,
                 this, &ProcessManagerDialog::onRemoveActionButtonClicked );
    }

    return button;
}

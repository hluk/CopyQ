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

#include "gui/processmanagerdialog.h"
#include "ui_processmanagerdialog.h"

#include "common/action.h"
#include "common/common.h"
#include "gui/iconfont.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"

#include <QDateTime>
#include <QPushButton>

namespace {

const int maxNumberOfProcesses = 100;

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
    return QDateTime::currentDateTime().toString(Qt::SystemLocaleLongDate);
}

class SortingGuard {
public:
    explicit SortingGuard(QTableWidget *table) : m_table(table) { m_table->setSortingEnabled(false); }
    ~SortingGuard() { m_table->setSortingEnabled(true); }
private:
    Q_DISABLE_COPY(SortingGuard)

    QTableWidget *m_table;
};

} // namespace

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

    QAction *act = new QAction(this);
    act->setShortcut(shortcutToRemove());
    connect( act, SIGNAL(triggered()),
             this, SLOT(onDeleteShortcut()) );
    addAction(act);
}

ProcessManagerDialog::~ProcessManagerDialog()
{
    delete ui;
}

void ProcessManagerDialog::actionAboutToStart(Action *action)
{
    Q_ASSERT(getRowForAction(action) == -1);

    const QString name = action->name();
    const QString command = action->command();

    createTableRow( name.isEmpty() ? command : name, action );

    QTableWidget *t = ui->tableWidgetCommands;

    t->item(0, tableCommandsColumns::name)->setToolTip(command);

    const QVariant id = action->property("COPYQ_ACTION_ID");
    QTableWidgetItem *statusItem = t->item(0, tableCommandsColumns::status);
    statusItem->setData(statusItemData::actionId, id);
    statusItem->setData(statusItemData::status, QProcess::Starting);

    // Limit rows in table.
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
    const int row = getRowForAction(action);
    Q_ASSERT(row != -1);

    QTableWidget *t = ui->tableWidgetCommands;
    SortingGuard sortGuard(t);

    QTableWidgetItem *statusItem = t->item(row, tableCommandsColumns::status);
    statusItem->setText(tr("Runnning"));
    statusItem->setData(statusItemData::status, QProcess::Running);
    updateTable();
}

void ProcessManagerDialog::actionFinished(Action *action)
{
    const int row = getRowForAction(action);
    Q_ASSERT(row != -1);

    const QString status = action->actionFailed() ? tr("Failed") : tr("Finished");

    QTableWidget *t = ui->tableWidgetCommands;
    SortingGuard sortGuard(t);

    QWidget *button = t->cellWidget(row, tableCommandsColumns::action);
    QTableWidgetItem *statusItem = t->item(row, tableCommandsColumns::status);
    statusItem->setText(status);
    statusItem->setData(statusItemData::status, QProcess::NotRunning);
    t->item(row, tableCommandsColumns::endTime)->setText(currentTime());
    button->setToolTip( tr("Remove") );
    button->setProperty( "text", QString(IconRemove) );
    updateTable();

    button->disconnect();
    connect( button, SIGNAL(clicked()),
             this, SLOT(onRemoveActionButtonClicked()) );

    // Reset action ID so it can be used again.
    t->item(row, tableCommandsColumns::status)->setData(statusItemData::actionId, 0);
    Q_ASSERT(getRowForAction(action) == -1);
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
    const int row = getRowForActionButton(sender());
    Q_ASSERT(row != -1);
    ui->tableWidgetCommands->removeRow(row);
}

void ProcessManagerDialog::onDeleteShortcut()
{
    const QList<QTableWidgetItem *> selectedItems = ui->tableWidgetCommands->selectedItems();

    QVector<int> rows( selectedItems.size() );

    foreach (QTableWidgetItem *item, selectedItems)
        rows.append( ui->tableWidgetCommands->row(item) );

    qSort( rows.begin(), rows.end(), qGreater<int>() );

    int lastRow = -1;
    foreach (int row, rows) {
        if (lastRow != row) {
            removeIfNotRunning(row);
            lastRow = row;
        }
    }
}

int ProcessManagerDialog::getRowForAction(Action *action) const
{
    QTableWidget *t = ui->tableWidgetCommands;
    const QVariant id = action->property("COPYQ_ACTION_ID");
    for (int row = 0; row < t->rowCount(); ++row) {
        if ( t->item(row, tableCommandsColumns::status)->data(statusItemData::actionId) == id )
            return row;
    }

    return -1;
}

int ProcessManagerDialog::getRowForActionButton(QObject *button) const
{
    QTableWidget *t = ui->tableWidgetCommands;
    for (int row = 0; row < t->rowCount(); ++row) {
        if ( t->cellWidget(row, tableCommandsColumns::action) == button )
            return row;
    }

    return -1;
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

void ProcessManagerDialog::createTableRow(const QString &name, Action *action)
{
    QTableWidget *t = ui->tableWidgetCommands;
    SortingGuard sortGuard(t);

    const QString status = action ? tr("Starting") : tr("Finished");

    t->insertRow(0);
    t->setItem( 0, tableCommandsColumns::name, new QTableWidgetItem(name) );
    t->setItem( 0, tableCommandsColumns::status, new QTableWidgetItem(status) );
    t->setItem( 0, tableCommandsColumns::beginTime, new QTableWidgetItem(currentTime()) );
    t->setItem( 0, tableCommandsColumns::endTime, new QTableWidgetItem(
                    action ? QString() : currentTime()) );
    t->setCellWidget( 0, tableCommandsColumns::action, createRemoveButton(action) );
    updateTable();
}

QWidget *ProcessManagerDialog::createRemoveButton(Action *action)
{
    QPushButton *button = new QPushButton(
                action ? QString(IconRemoveSign) : QString(IconRemove));

    button->setFlat(true);
    button->setFont(iconFont());
    button->setToolTip(tr("Terminate"));
    button->setFocusPolicy(Qt::NoFocus);

    if (action) {
        connect( button, SIGNAL(clicked()),
                 action, SLOT(terminate()) );
    } else {
        connect( button, SIGNAL(clicked()),
                 this, SLOT(onRemoveActionButtonClicked()) );
    }

    return button;
}

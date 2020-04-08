/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#include "gui/actionhandlerdialog.h"
#include "ui_actionhandlerdialog.h"

#include "common/actionhandlerenums.h"
#include "gui/actionhandler.h"

#include <QSortFilterProxyModel>
#include <QSet>

namespace {

void terminateSelectedActions(QItemSelectionModel *selectionModel, ActionHandler *actionHandler)
{
     QSet<int> ids;
     for ( const auto &index : selectionModel->selectedIndexes() ) {
         const int actionId = index.data(Qt::UserRole + ActionHandlerRole::id).toInt();
         ids.insert(actionId);
     }
     for (const int id : ids)
         actionHandler->terminateAction(id);
}

void updateTerminateButton(QItemSelectionModel *selectionModel, QAbstractItemModel *model, QPushButton *button)
{
     for ( const auto &index : selectionModel->selectedIndexes() ) {
         const int row = index.row();
         const auto statusIndex = model->index(row, 0);
         const auto state = static_cast<ActionState>(statusIndex.data(Qt::UserRole + ActionHandlerRole::status).toInt());
         if (state == ActionState::Running || state == ActionState::Starting) {
             button->setEnabled(true);
             return;
         }
     }

     button->setEnabled(false);
}

} // namespace

ActionHandlerDialog::ActionHandlerDialog(ActionHandler *actionHandler, QAbstractItemModel *model, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ActionHandlerDialog)
{
    ui->setupUi(this);

    auto proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortRole(Qt::UserRole + ActionHandlerRole::sort);
    proxyModel->setFilterKeyColumn(ActionHandlerColumn::name);
    ui->tableView->setModel(proxyModel);

    ui->tableView->resizeColumnsToContents();

    ui->tableView->sortByColumn(ActionHandlerColumn::status, Qt::DescendingOrder);

    connect( ui->filterLineEdit, &QLineEdit::textChanged, proxyModel,
             [proxyModel](const QString &pattern) {
#if QT_VERSION >= QT_VERSION_CHECK(5,12,0)
                 const QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
                 proxyModel->setFilterRegularExpression(re);
#else
                 const QRegExp re(pattern, Qt::CaseInsensitive);
                 proxyModel->setFilterRegExp(re);
#endif
             } );

    const auto selectionModel = ui->tableView->selectionModel();
    connect( ui->terminateButton, &QPushButton::clicked, this,
             [selectionModel, actionHandler]() {
                 terminateSelectedActions(selectionModel, actionHandler);
             } );

    const auto updateTerminateButtonSlot =
        [this, selectionModel, proxyModel]() {
            updateTerminateButton(selectionModel, proxyModel, ui->terminateButton);
        };

    connect( model, &QAbstractItemModel::dataChanged, this, updateTerminateButtonSlot );
    connect( selectionModel, &QItemSelectionModel::selectionChanged, this, updateTerminateButtonSlot );
}

ActionHandlerDialog::~ActionHandlerDialog()
{
    delete ui;
}

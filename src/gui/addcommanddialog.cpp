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

#include "addcommanddialog.h"
#include "ui_addcommanddialog.h"

#include "common/command.h"
#include "common/mimetypes.h"
#include "common/predefinedcommands.h"
#include "common/shortcuts.h"
#include "common/textdata.h"
#include "item/itemfactory.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"
#include "platform/platformnativeinterface.h"

#include <QAbstractListModel>
#include <QLocale>
#include <QSortFilterProxyModel>

namespace {

class CommandModel final : public QAbstractListModel {
public:
    explicit CommandModel(const QVector<Command> &commands, QObject *parent = nullptr)
        : QAbstractListModel(parent)
        , m_commands(commands)
    {
    }

    int rowCount(const QModelIndex &) const override
    {
        return m_commands.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid())
            return QVariant();

        if (role == Qt::DecorationRole) {
            const QString &icon = m_commands[index.row()].icon;

            QVariant iconOrIconId;
            if (icon.size() == 1)
                iconOrIconId = static_cast<uint>(icon[0].unicode());
            else
                iconOrIconId = QIcon(icon);

            return getIcon(iconOrIconId);
        }
        if (role == Qt::DisplayRole)
            return m_commands[index.row()].name;
        if (role == Qt::UserRole)
            return QVariant::fromValue(m_commands[index.row()]);

        return QVariant();
    }

private:
    QVector<Command> m_commands;
};

} // namespace

AddCommandDialog::AddCommandDialog(const QVector<Command> &pluginCommands, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddCommandDialog)
    , m_filterModel(new QSortFilterProxyModel(this))
{
    ui->setupUi(this);

    connect(ui->lineEditFilter, &QLineEdit::textChanged,
            this, &AddCommandDialog::onLineEditFilterTextChanged);
    connect(ui->listViewCommands, &QListView::activated,
            this, &AddCommandDialog::onListViewCommandsActivated);

    QAbstractItemModel *model = new CommandModel(predefinedCommands() + pluginCommands, m_filterModel);
    m_filterModel->setSourceModel(model);
    ui->listViewCommands->setModel(m_filterModel);
    ui->listViewCommands->setCurrentIndex(m_filterModel->index(0, 0));

    WindowGeometryGuard::create(this);
}

AddCommandDialog::~AddCommandDialog()
{
    delete ui;
}

void AddCommandDialog::accept()
{
    const QModelIndexList indexes =
            ui->listViewCommands->selectionModel()->selectedIndexes();

    if (!indexes.isEmpty()) {
        QVector<Command> commands;

        commands.reserve( indexes.size() );
        for (const auto &index : indexes)
            commands.append( index.data(Qt::UserRole).value<Command>() );

        emit addCommands(commands);
    }

    QDialog::accept();
}

void AddCommandDialog::onLineEditFilterTextChanged(const QString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,12,0)
    const QRegularExpression re(text, QRegularExpression::CaseInsensitiveOption);
    m_filterModel->setFilterRegularExpression(re);
#else
    const QRegExp re(text, Qt::CaseInsensitive);
    m_filterModel->setFilterRegExp(re);
#endif
}

void AddCommandDialog::onListViewCommandsActivated()
{
    accept();
}

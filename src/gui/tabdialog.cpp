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

#include "tabdialog.h"
#include "ui_tabdialog.h"

#include <QPushButton>

TabDialog::TabDialog(TabDialog::TabDialogType type, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TabDialog)
    , m_tabGroupName()
    , m_tabs()
{
    ui->setupUi(this);

    if (type == TabNew) {
        setWindowTitle( tr("New Tab") );
        setWindowIcon( QIcon(":/images/tab_new") );
    } else if (type == TabRename) {
        setWindowTitle( tr("Rename Tab") );
        setWindowIcon( QIcon(":/images/tab_rename") );
    } else {
        setWindowTitle( tr("Rename Tab Group") );
        setWindowIcon( QIcon(":/images/tab_rename") );
    }

    connect( this, &TabDialog::accepted,
             this, &TabDialog::onAccepted );

    connect( ui->lineEditTabName, &QLineEdit::textChanged,
             this, &TabDialog::validate );

    validate();
}

TabDialog::~TabDialog()
{
    delete ui;
}

void TabDialog::setTabIndex(int tabIndex)
{
    m_tabIndex = tabIndex;
}

void TabDialog::setTabs(const QStringList &tabs)
{
    m_tabs = tabs;
    validate();
}

void TabDialog::setTabName(const QString &tabName)
{
    ui->lineEditTabName->setText(tabName);
    ui->lineEditTabName->selectAll();
}

void TabDialog::setTabGroupName(const QString &tabGroupName)
{
    m_tabGroupName = tabGroupName;
    ui->lineEditTabName->setText(m_tabGroupName);
}

void TabDialog::validate()
{
    bool ok = true;
    const QString text = ui->lineEditTabName->text();

    if ( m_tabGroupName.isEmpty() ) {
        ok = !text.isEmpty() && !m_tabs.contains(text);
    } else {
        const QString tabPrefix = m_tabGroupName + '/';
        for (const auto &tab : m_tabs) {
            if ( tab == m_tabGroupName || tab.startsWith(tabPrefix) ) {
                const QString newName = text + tab.mid(m_tabGroupName.size());
                if ( newName.isEmpty() || m_tabs.contains(newName) ) {
                    ok = false;
                    break;
                }
            }
        }
    }

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

void TabDialog::onAccepted()
{
    const auto newName = ui->lineEditTabName->text();

    if ( m_tabGroupName.isEmpty() && m_tabIndex == -1 )
        emit newTabNameAccepted(newName);
    else if ( m_tabGroupName.isEmpty() )
        emit barTabNameAccepted(newName, m_tabIndex);
    else
        emit treeTabNameAccepted(newName, m_tabGroupName);
}
